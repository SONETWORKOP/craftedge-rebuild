import os
import hashlib
import zipfile
import signal
import platform
import shutil
from functools import partial
from threading import Event
from urllib.request import Request, urlopen
from rich.progress import (
    BarColumn,
    DownloadColumn,
    Progress,
    TextColumn,
    TransferSpeedColumn,
)
from util import (
    load_conf,
    save_conf,
    NS_DEV_MAT_SRC_SHA256,
    NS_DEV_MAT_SRC_URL,
    NS_DEV_SHADERC_ASSETS,
    NS_DEV_SHADERC_URL_PREFIX,
)

done_event = Event()


def handle_sigint(signum, frame):
    done_event.set()


signal.signal(signal.SIGINT, handle_sigint)


progress = Progress(
    TextColumn("[bold blue]{task.fields[filename]}", justify="right"),
    BarColumn(bar_width=None),
    "•",
    DownloadColumn(),
    "•",
    TransferSpeedColumn(),
)


def _download_file(url: str, path: str, filename: str, sha256: str | None = None) -> None:
    task_id = progress.add_task("download", filename=filename, start=False)
    request = Request(
        url,
        headers={"Accept": "application/octet-stream", "User-Agent": "newb-pack-builder"},
    )
    temp_path = path + ".download"
    digest = hashlib.sha256()
    try:
        with urlopen(request) as response, open(temp_path, "wb") as dest_file:
            content_length = response.info().get("Content-length")
            progress.update(task_id, total=int(content_length) if content_length else None)
            progress.start_task(task_id)
            for data in iter(partial(response.read, 32768), b""):
                dest_file.write(data)
                digest.update(data)
                progress.update(task_id, advance=len(data))
                if done_event.is_set():
                    raise KeyboardInterrupt
        if sha256 is not None and digest.hexdigest() != sha256:
            raise ValueError(f"Checksum mismatch for {filename}")
        os.replace(temp_path, path)
    finally:
        if os.path.exists(temp_path):
            os.remove(temp_path)


def get_shaderc_url(data_path: str, os_name: str, arch: str):
    shaderc_path = os.path.join(data_path, "shaderc")

    if os_name == 'Windows':
        shaderc_asset = "win-x64.exe"
        shaderc_path += ".exe"
    elif os_name == "Darwin":
        shaderc_asset = "osx-x64"
    elif os_name == "Linux" or os_name == "Android":
        if arch == 'x86_64':
            shaderc_asset = "linux-x64"
        elif arch in ['aarch64']:
            shaderc_asset = "android-arm64"
        elif arch in ['armv7l', 'armv8l']:
            shaderc_asset = "android-arm"
        else:
            progress.console.print("No shaderc version found for", arch, style='red')
            return None
    else:
        progress.console.print("Unable to determine platform", os_name, style='red')
        return None
    shaderc_url = NS_DEV_SHADERC_URL_PREFIX + NS_DEV_SHADERC_ASSETS[shaderc_asset]
    return (shaderc_url, shaderc_path, "shaderc-" + shaderc_asset)


def check_and_apply_termux_fix():
    # libc++_shared.so not found fix for termux
    lib_path = "tool/lib"
    if not os.path.exists(lib_path):
        os.mkdir(lib_path)

    termux_lib_file = "/data/data/com.termux/files/usr/lib/libc++_shared.so"
    if os.path.exists(termux_lib_file) and not os.path.exists(lib_path + "/libc++_shared.so"):
        progress.console.print("Adding termux fix for libc++_shared.so not found")
        shutil.copyfile(termux_lib_file, lib_path + "/libc++_shared.so")


def run(args):
    conf = load_conf()

    os_name = platform.system()
    arch = platform.machine()

    data_path = os.path.join('tool', 'data')
    mat_path = os.path.join(data_path, 'materials')

    shaderc_details = get_shaderc_url(data_path, os_name, arch)
    if shaderc_details is None:
        exit(1)
    shaderc_url, shaderc_path, shaderc_filename = shaderc_details

    if args.reset:
        shutil.rmtree(data_path, ignore_errors=True)

    if not os.path.exists(data_path):
        os.mkdir(data_path)
    if not os.path.exists(mat_path):
        os.mkdir(mat_path)

    if os_name == "Android":
        check_and_apply_termux_fix()

    # compare with existing setup, remove if update is needed
    if conf.get("shaderc_url") != shaderc_url and os.path.exists(shaderc_path):
        os.remove(shaderc_path)
    if conf.get("mat_src_url") != NS_DEV_MAT_SRC_URL:
        shutil.rmtree(mat_path, ignore_errors=True)

    with progress:
        if not os.path.exists(shaderc_path):
            progress.console.print("Downloading shaderc")
            _download_file(shaderc_url, shaderc_path, shaderc_filename)
            os.chmod(shaderc_path, 0o755)

        test_mat = os.path.join(mat_path, "Sky.material.json")
        if not os.path.exists(test_mat):
            progress.console.print("Downloading source materials")
            mat_filename = os.path.join(data_path, 'materials.zip')
            _download_file(
                NS_DEV_MAT_SRC_URL,
                mat_filename,
                "src-materials-1.26.10.zip",
                NS_DEV_MAT_SRC_SHA256,
            )
            with zipfile.ZipFile(mat_filename, 'r') as zip_ref:
                zip_ref.extractall(mat_path)
            os.remove(mat_filename)

    conf["arch"] = arch
    conf["os_name"] = os_name
    conf["mat_src_url"] = NS_DEV_MAT_SRC_URL
    conf["shaderc_url"] = shaderc_url
    conf["shaderc_url_prefix"] = NS_DEV_SHADERC_URL_PREFIX
    save_conf(conf)

    progress.console.print("[bold green]All done!")

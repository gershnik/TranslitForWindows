#  Copyright (c) 2023, Eugene Gershnik
#  SPDX-License-Identifier: GPL-3.0-or-later

import sys

if sys.version_info < (3, 11):
    raise RuntimeError('Python 3.11 or greater is required to build this project')

import shutil
import subprocess
import tarfile
from pathlib import Path, PurePath
from urllib.request import urlretrieve
from urllib.error import HTTPError

components = {
    'WebView2': {
        'ver': '1.0.3296.44',
        'nuget': 'Microsoft.Web.WebView2',
        'dir': 'WebView2',
        'verFile': 'fetch.version.txt',
        'generateVerFile': True
    },
    'rapidjson': {
        'ver': '1.1.0',
        'url': 'https://github.com/Tencent/rapidjson/archive/refs/tags/v{ver}/v{ver}.tar.gz',
        'unpacker': {
            'type': 'tar',
            'strip': 1
        },
        'dir': 'rapidjson',
        'verFile': 'fetch.version.txt',
        'generateVerFile': True
    },
    'isptr': {
        'ver': '1.9',
        'url': 'https://github.com/gershnik/intrusive_shared_ptr/archive/refs/tags/v{ver}/v{ver}.tar.gz',
        'unpacker': {
            'type': 'tar',
            'strip': 1
        },
        'dir': 'isptr',
        'verFile': 'fetch.version.txt',
        'generateVerFile': True
    },
    'muuid': {
        'ver': '1.8',
        'url': 'https://github.com/gershnik/modern-uuid/archive/refs/tags/v{ver}/v{ver}.tar.gz',
        'unpacker': {
            'type': 'tar',
            'strip': 1
        },
        'dir': 'muuid',
        'verFile': 'fetch.version.txt',
        'generateVerFile': True
    },
    'sys_string': {
        'ver': '3.4',
        'url': 'https://github.com/gershnik/sys_string/archive/refs/tags/v{ver}/v{ver}.tar.gz',
        'unpacker': {
            'type': 'tar',
            'strip': 1
        },
        'dir': 'sys_string',
        'verFile': 'fetch.version.txt',
        'generateVerFile': True
    }
}

MYDIR = Path(__file__).parent
EXTERNAL_DIR = MYDIR / 'External'

    
def get_version_from_file(path):
    existing_ver = None
    if path.is_file():
        existing_ver = path.read_text().rstrip()
    return existing_ver

def check_up_to_date(name, component):
    directory = EXTERNAL_DIR / component['dir']
    up_to_date = False
    
    if component.get('verFile'):
        ver_file = directory / component['verFile']
        required_ver = component['ver']
        existing_ver = get_version_from_file(ver_file)
        if existing_ver == required_ver:
            up_to_date = True
    else:
        print(f"Dont know how to detect version of {name}", file=sys.stderr)
        sys.exit(1)
        
    if not up_to_date:
        print(f'{name} not up to date, required {required_ver}, existing {existing_ver}')
    else:
        print(f'{name} is up to date')
    return up_to_date
 

def untar(name, component, archive):
    directory = EXTERNAL_DIR / component.get('dir', name)
    strip = component['unpacker'].get('strip')

    def extractor(members):
        for tarinfo in members:
            if strip is not None:
                path = PurePath(tarinfo.name)
                if len(path.parts) <= strip:
                    continue
                modified = '/'.join(path.parts[strip:])
                tarinfo.name = modified
            yield tarinfo

    tar = tarfile.open(archive, "r")
    tar.extractall(path=directory, members=extractor(tar))

def copy_archive(name, component, archive):
    directory = EXTERNAL_DIR / component.get('dir', name)
    directory.mkdir(parents=True, exist_ok=True)
    shutil.copy(archive, directory / name)

def nuget(name, component, unpacker):
    directory = EXTERNAL_DIR / component.get('dir', name)
    directory.mkdir(parents=True, exist_ok=True)
    subprocess.run([EXTERNAL_DIR / 'nuget/nuget.exe', 'install', component['nuget'], '-Version', component['ver'], '-ExcludeVersion', '-OutputDirectory', directory], check=True)
    generate_ver_file = component.get('generateVerFile', False)
    if generate_ver_file:
        ver_file = directory / component['verFile']
        ver_file.write_text(component['ver'])

def fetch_url(name, component, unpacker):
    directory = EXTERNAL_DIR / component['dir']
    
    shutil.rmtree(directory, ignore_errors=True)
    if directory.exists():
        print(f'Unable to remove {name} directory', file=sys.stderr)
        sys.exit(1)
    directory.mkdir()
    url = component['url'].format(ver = component['ver'])
    try:
        path, _ = urlretrieve(url)
    except HTTPError as err:
        print(f'Uanble to download {url}: {err}', file=sys.stderr)
        sys.exit(1)
    try:
        unpacker(name, component, Path(path))
        print(f'Installed {name}')
    finally:
        Path(path).unlink()
    generate_ver_file = component.get('generateVerFile', False)
    if generate_ver_file:
        ver_file = directory / component['verFile']
        ver_file.write_text(component['ver'])

def get_fetcher(name, component):
    if component.get('url'):
        return fetch_url
    
    if component.get('nuget'):
        return nuget
    
    print(f"Dont know how to download {name}", file=sys.stderr)
    sys.exit(1)

def get_unpacker(name, component):
    if component.get('nuget'):
        return None
    
    component_type = component['unpacker']['type']
    if component_type == 'tar':
        return untar
    
    
    print(f"Dont know how to unpack {name}", file=sys.stderr)
    sys.exit(1)

def main():
    EXTERNAL_DIR.mkdir(parents=True, exist_ok=True)

    nuget = {
        'ver': '6.14.0',
        'url': 'https://dist.nuget.org/win-x86-commandline/v{ver}/nuget.exe',
        'dir': 'nuget',
        'verFile': 'VERSION',
        'generateVerFile': True
    }
    fetch_url('nuget.exe', nuget, copy_archive)

    for name, component in components.items():
        if check_up_to_date(name, component):
            continue
        fetcher = get_fetcher(name, component)
        unpacker = get_unpacker(name, component)
        fetcher(name, component, unpacker)

if __name__ == '__main__':
    main()

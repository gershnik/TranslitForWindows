#  Copyright (c) 2023, Eugene Gershnik
#  SPDX-License-Identifier: GPL-3.0-or-later

import sys
import argparse
import subprocess
import shutil
import xml.etree.ElementTree as ET

from pathlib import Path

MYDIR = Path(__file__).parent
SRCDIR = MYDIR.parent.absolute()

def build(command: str, config: str, arch: str, builddir:Path):
    msbuild = 'msbuild'

    intdir = builddir / "IntDir/Installer" / arch / config / 'wix'
    outdir = builddir / "Installers" / config
    msi = outdir / f"Translit-{arch.lower()}.msi"

    if command == 'clean' or command == 'rebuild':
        print('\nInstaller: Cleaning\n', flush=True)
        shutil.rmtree(intdir, ignore_errors=True)
        if (intdir.exists()):
            print(f'Unable to remove {intdir}', file=sys.stderr)
            return 1
        msi.unlink(missing_ok=True)
        msi.with_suffix('.wixpdb').unlink(missing_ok=True)
        print('\n', flush=True)
    
    if command == 'clean':
        return 0
    
    props = ET.parse(SRCDIR / 'Version.props')
    namespaces = {'': 'http://schemas.microsoft.com/developer/msbuild/2003'}
    macros = props.find('./PropertyGroup[@Label="UserMacros"]', namespaces)
    major = macros.find('BuildMajorVersion', namespaces).text
    minor = macros.find('BuildMinorVersion', namespaces).text
    patch = macros.find('BuildPatchVersion', namespaces).text
    print(f'Installer: Version: {major}.{minor}.{patch}', flush=True)

    print('Installer: Building x86\n', flush=True)
    subprocess.run([msbuild, SRCDIR / 'Translit.sln', 
                    '/t:Settings', '/p:Configuration=Release;Platform=x86',
                    '-v:m'], check=True)
    print('\n', flush=True)

    
    print(f'Installer: Building {arch}\n', flush=True)
    subprocess.run([msbuild, SRCDIR / 'Translit.sln', 
                    '/t:Settings', f'/p:Configuration=Release;Platform={arch}',
                    '-v:m'], check=True)
    print('\n', flush=True)

    print('Installer: Building MSI...', flush=True)
    subprocess.run(['wix', 'build',
        '-ext', 'WixToolset.UI.wixext',
        '-ext', 'WixToolset.Util.wixext',
        '-arch', arch,
        '-d', f'TLVersion={major}.{minor}.{patch}',
        '-d', f'TLProductBinaries64={builddir / "Out" / arch / config}',
        '-d', f'TLProductBinariesX86={builddir / "Out/Win32" / config}',
        '-d', f'TLSourceRoot={SRCDIR}',
        '-intermediateFolder', f'{builddir / "IntDir/Installer" / arch / config}',
        '-culture', 'en-US',
        '-loc', 'Strings.wxl',
        '-out', msi,
        'Package.wxs'], check=True)
    print('\n', flush=True)

    print('Installer: Validating MSI...', flush=True)
    subprocess.run(['wix', 'msi', 'validate', msi], check=True)
    print('\n', flush=True)


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument('command', type=str, choices=['build', 'clean', 'rebuild'])
    parser.add_argument('config', type=str, choices=['Debug', 'Release'])
    parser.add_argument('arch', type=str, choices=['x64', 'ARM64'])
    parser.add_argument('builddir', type=Path)
    
    args = parser.parse_args()

    command: str = args.command
    arch: str = args.arch
    config: str = args.config
    builddir:Path = args.builddir

    try:
        build(command, config, arch, builddir)
    except subprocess.CalledProcessError as ex:
        print(f'{ex}', file=sys.stderr)
        return 1
        

if __name__ == '__main__':
    sys.exit(main())

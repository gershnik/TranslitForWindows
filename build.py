
import sys
import os
import argparse
import subprocess
import shutil
import xml.etree.ElementTree as ET

from pathlib import Path


MYDIR = Path(__file__).parent

def elementOrDie(el: ET.Element | None):
    if el is None:
        raise RuntimeError('Malformed Get-CimInstance response')
    return el

def get_vs_instances():
    vsinfo = subprocess.run([
        'powershell',
        'Get-CimInstance MSFT_VSInstance -Namespace root/cimv2/vs | ConvertTo-Xml -As String'
        ], check=True, capture_output=True).stdout
    
    props = ET.fromstring(vsinfo)

    instances = props.findall('./Object[@Type="Microsoft.Management.Infrastructure.CimInstance"]')

    ret = []
    for instance in instances:
        name = elementOrDie(instance.find('./Property[@Name="Name"]')).text
        version = elementOrDie(instance.find('./Property[@Name="Version"]')).text
        location = elementOrDie(instance.find('./Property[@Name="InstallLocation"]')).text
        product_id = elementOrDie(instance.find('./Property[@Name="ProductId"]')).text

        if location is None or version is None:
            raise RuntimeError('Malformed Get-CimInstance response')
        
        location = Path(location)
        version = tuple(int(part) for part in version.split('.'))
        ret.append({'version': version, 'product_id': product_id, 'name': name, 'location': location})

    ret.sort(key= lambda x: x['version'], reverse=True)
    return ret

def main():

    arch = os.environ.get('PROCESSOR_ARCHITECTURE')
    if arch is None:
        raise RuntimeError('Cannot find PROCESSOR_ARCHITECTURE environment variable')
        
    arch = arch.lower()

    studios = get_vs_instances()
    studio_dir = None
    for studio in studios:
        if studio['version'][0] == 17:
            print(f'Using {studio["name"]}', flush=True)
            studio_dir = studio['location']
    if studio_dir is None:
        raise RuntimeError('Cannot find Visual Studio 17 instance')
    
    msbuild = studio_dir / f'MSBuild\\Current\\Bin\\{arch}\\MSBuild.exe'

    subprocess.run([msbuild, MYDIR / 'Translit.sln', 
                    '/t:Installer', '/p:Configuration=Release;Platform=x64',
                    '-v:m'], check=True)
    
    subprocess.run([msbuild, MYDIR / 'Translit.sln', 
                    '/t:Installer', '/p:Configuration=Release;Platform=ARM64',
                    '-v:m'], check=True)

if __name__ == '__main__':
    sys.exit(main())
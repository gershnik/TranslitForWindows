#  Copyright (c) 2023, Eugene Gershnik
#  SPDX-License-Identifier: GPL-3.0-or-later

import sys
import xml.etree.ElementTree as ET

from pathlib import Path

MYDIR = Path(__file__).parent
SRCDIR = MYDIR.parent.absolute()

def elementOrDie(el: ET.Element | None):
    if el is None:
        print('Malformed Version.props', file=sys.stderr)
        sys.exit(1)
    return el


props = ET.parse(SRCDIR / 'Version.props')
namespaces = {'': 'http://schemas.microsoft.com/developer/msbuild/2003'}
macros = elementOrDie(props.find('./PropertyGroup[@Label="UserMacros"]', namespaces))
major = elementOrDie(macros.find('BuildMajorVersion', namespaces)).text
minor = elementOrDie(macros.find('BuildMinorVersion', namespaces)).text
patch = elementOrDie(macros.find('BuildPatchVersion', namespaces)).text
print(f'{major}.{minor}.{patch}')


# Copyright (c) 2023, Eugene Gershnik
# SPDX-License-Identifier: GPL-3.0-or-later


import sys
import tomllib
import unicodedata
import re
import textwrap
from pathlib import Path
from typing import Any
from dataclasses import dataclass

MYDIR = Path(__file__).parent
ROOTDIR = MYDIR.parent

def make_macro_from_filename(filename: str):
    filename = filename.replace('.', '_').replace('-', '_')
    converted = ""
    prev_lower = False
    for c in filename:
        if c.isupper() and prev_lower:
            converted += '_'
            converted += c
        elif c in ['.', '-', '+']:
            converted += '_'
        else:
            converted += c.upper()
        prev_lower = c.islower()
    return converted

def quote_cpp_string(text: str):
    ret = ''
    for c in text:
        if c == '"':
            c = '\\"'
        ret += c
    return ret

def make_mapper_name(language: str, variant: str):
    prefix = f'g_mapper{language.title()}'
    words = re.split(r'[-_]', variant)
    words = [w.title() for w in words]
    suffix = ''.join(words)
    return prefix + suffix

def make_html_name(language: str, variant: str):
    prefix = f'g_html{language.title()}'
    words = re.split(r'[-_]', variant)
    words = [w.title() for w in words]
    suffix = ''.join(words)
    return prefix + suffix

def make_div_name(language: str, variant: str):
    ret = f'mapping-{language}'
    if variant != 'default':
        ret += f'_{variant}'
    return ret

def write_file_if_different(path: Path, content: str):
    bin_content = content.encode('utf-8')
    if path.is_file():
        existing = path.read_bytes()
        if existing == bin_content:
            print(f'  {path.relative_to(ROOTDIR)} - up to date')
            return
    path.write_bytes(bin_content)
    print(f'  {path.relative_to(ROOTDIR)} - written')

def get_execution_mappings(varname: str, section: dict[str, Any]):
    for dst, data in section.items():
        if isinstance(data, dict):
            data = data[varname]
        
        if isinstance(data, str):
            yield dst, data
        elif isinstance(data, list):
            for srcitem in data:
                if isinstance(srcitem, str):
                    yield dst, srcitem
                elif isinstance(srcitem, list):
                    for srcsubitem in srcitem:
                        if isinstance(srcsubitem, str):
                            yield dst, srcsubitem
                        else:
                            raise RuntimeError(f'invalid mapping structure for {dst}')
                else:
                    raise RuntimeError(f'invalid mapping structure for {dst}')
        else:
            raise RuntimeError(f'invalid mapping structure for {dst}')

def get_presentation_destinations(varname: str, section: dict[str, Any], overrides: dict[str, Any]):
    for dst in section:
        override = overrides.get(dst)
        if override is not None:
            if isinstance(override, dict):
                override = override[varname]
            dst = override[0]
        if len(dst) > 1:
            yield dst, None
        elif unicodedata.combining(dst) != 0:
            yield '&#160;' + dst, 'combining'
        else:
            yield dst, None

def get_presentation_mappings(varname: str, section: dict[str, Any], overrides: dict[str, Any]):
    for dst, data in section.items():
        override = overrides.get(dst)
        if override is not None:
            if isinstance(override, dict):
                override = override[varname]
            data = override[1]

        if isinstance(data, dict):
            data = data[varname]

        if isinstance(data, str):
            yield data
        elif isinstance(data, list):
            if len(data) == 1:
                if isinstance(data[0], str):
                    yield data[0]
                elif isinstance(data[0], list):
                    yield data[0][0]
            else:
                res = []
                for srcitem in data:
                    if isinstance(srcitem, str):
                        res.append(srcitem)
                    elif isinstance(srcitem, list):
                        res.append(srcitem[0])
                yield res

def dedent(text: str):
    max_common_spaces = 2**32
    lines = text.splitlines()
    if len(lines) == 0:
        return text
    for line in lines:
        if len(line) == 0:
            continue
        spaces = 0
        for c in line:
            if not c.isspace():
                break
            spaces += 1
        max_common_spaces = min(spaces, max_common_spaces)
    ret = ''
    for line in lines[:-1]:
        ret += line[max_common_spaces:] + '\n'
    ret += lines[-1][max_common_spaces:]
    if text[-1] == '\n':
        ret += '\n'
    return ret
    
def indent_insert(text: str, count: int):
    ret = textwrap.indent(text, ' ' * count, lambda line: True).lstrip()
    return ret
        

def generate_mapping_header(config: dict[str, Any]):
    language: str = config['language']
    header = f'Table{language.upper()}.hpp'

    macro = make_macro_from_filename(header)
    content = dedent(f'''\
        // Copyright (c) 2023, Eugene Gershnik
        // SPDX-License-Identifier: GPL-3.0-or-later
                     
        // THIS FILE IS AUTO-GENERATED. DO NOT EDIT.

        #ifndef TRANSLIT_HEADER_{macro}_INCLUDED
        #define TRANSLIT_HEADER_{macro}_INCLUDED

        #include <Mapper/Mapper.hpp>

        ''')
    
    variants: dict[str, Any] = config['variants']
    mappings: list[dict[str, Any]] = config['mappings']

    
    for varidx, varname in enumerate(variants):
        if varidx > 0:
            content += '\n'

        variable_name = make_mapper_name(language, varname)

        content += dedent(f'''\
            template<std::ranges::forward_range Range>
            constexpr auto {variable_name} = makePrefixMapper<Range,
            ''')
        line_count = 0
        for section in mappings:
            for dst, src in get_execution_mappings(varname, section):
                if line_count > 0:
                    content += ',\n'
                content += f"    Mapping{{u'{dst}', u\"{quote_cpp_string(src)}\"}}"
                line_count += 1
        content += '\n>();\n'

    content += '\n#endif\n'
    tabledir = ROOTDIR / 'Translit\\tables'
    tabledir.mkdir(exist_ok=True)
    write_file_if_different(tabledir / header, content)

def generate_div(config: dict[str, Any]):

    variants: dict[str, Any] = config['variants']
    mappings: list[dict[str, Any]] = config['mappings']
    overrides: dict[str, Any] = config.get('display_override', {})

    if config.get('rtl', False):
        rtl = ' dir="rtl"'
    else:
        rtl = ''

    language: str = config['language']

    content = ''
    
    for varname in variants:
        divname = make_div_name(language, varname)
        content += dedent(f'''\
                    <div id="{divname}"{rtl} style="width:100%;display:none">
            ''')
        for section_idx, section in enumerate(mappings):
            if section_idx > 0:
                content += '            <br>\n'
            content += dedent('''\
                        <table class="main">
                            <tr class="headerRow">
            ''')
            for idx, (dst, clazz) in enumerate(get_presentation_destinations(varname, section, overrides)):
                if idx > 0:
                    content += '\n'
                if clazz is not None:
                    content += f'                    <td class="{clazz}">{dst}</td>'
                else:
                    content += f'                    <td>{dst}</td>'
            content += dedent('''
                            </tr>
                            <tr class="contentRow">
            ''')
            for idx, data in enumerate(get_presentation_mappings(varname, section, overrides)):
                if idx > 0:
                    content += '\n'
                
                if isinstance(data, str):
                    content += f'                    <td>{data}</td>'
                elif isinstance(data, list):
                    content += '                    <td><table class="multi">'
                    for srcitem in data:
                        content += f'<tr><td>{srcitem}</td></tr>'
                    content += '</table></td>'
            content += dedent('''
                            </tr>
                        </table>
            ''')
        
        content += '            <div style="height:0.5em"></div>\n'
        footer = config.get('footer', {}).get('content')
        if footer is not None:
            footer = footer.rstrip()
            content += dedent(f'''\
                        <div class="footer">
                            <span>
                                {indent_insert(footer, 32)}
                            </span>
                        </div>
                        <div style="height:0.5em"></div>
            ''')
        content += dedent('''\
                    </div>
            ''')
    return content

def generate_html(content: str):
    html_path = ROOTDIR / 'Settings\\res\\main.html'
    html = html_path.read_text(encoding='utf-8')
    html_start = html.find('<!-- THE TABLES BELOW ARE AUTO-GENERATED. DO NOT EDIT. -->')
    if html_start == -1:
        raise RuntimeError(f'Invalid main.html')
    html_end = html.find('<!-- END OF AUTO-GENERATED SECTION. -->', html_start)
    if html_end == -1:
        raise RuntimeError(f'Invalid main.html')
    
    content += '        '
    write_file_if_different(html_path, html[:html_start] + content + html[html_end:])

def generate_markdown(config):

    variants: dict[str, Any] = config['variants']
    mappings: list[dict[str, Any]] = config['mappings']
    overrides: dict[str, Any] = config.get('display_override', {})

    if config.get('rtl', False):
        rtl = ' dir="rtl"'
    else:
        rtl = ''

    content = dedent(f'''\
        <!-- THIS FILE IS AUTO-GENERATED. DO NOT EDIT. -->
        # {config["name"]} Mappings
        ''')
    for varname, vardata in variants.items():
        if len(variants) > 1:
            content += f'''

## {vardata['display']}

'''
        for section_idx, section in enumerate(mappings):
            if section_idx > 0:
                content += '\n'
            content += f'<div{rtl}><table><tr>\n'
            for dst, _ in get_presentation_destinations(varname, section, overrides):
                content += f'<td>{dst}</td>'
            content += '\n</tr><tr>\n'
            for data in get_presentation_mappings(varname, section, overrides):
                if isinstance(data, str):
                    content += f'<td>{data}</td>'
                elif isinstance(data, list):
                    content += '<td>' + '<br>'.join(data) + '</td>'

            content += '\n</tr></table></div>'
    
    language: str = config['language']
    filename = f'mapping-{language}.md'
    (ROOTDIR / 'doc').mkdir(parents=True, exist_ok=True)
    write_file_if_different(ROOTDIR / 'doc' / filename, content)


@dataclass
class ImplVar:
    name: str
    display_name: str

@dataclass
class ImplLang:
    name: str
    variants: list[ImplVar]

@dataclass
class Impl:
    headers: list[str]
    languages: dict[str, ImplLang]

def generate_implementation(impl: Impl):
    content = dedent('''\
        // Copyright (c) 2023, Eugene Gershnik
        // SPDX-License-Identifier: GPL-3.0-or-later
                     
        // THIS FILE IS AUTO-GENERATED. DO NOT EDIT.

        #include <Translit/Languages.h>
        #include <Translit/Identifiers.h>

        #include <Mapper/Transliterator.hpp>

        #include "../res/resource.h"

        ''')
    content += '\n'.join([f'#include "{header}"' for header in impl.headers])
    content += dedent('''

        using MappingFunc = Transliterator::MappingFunc;
        using Range = Transliterator::Range;

        struct RealMappingInfo : public MappingInfo {
            Transliterator::MappingFunc * mapper;
        };

        template<size_t N>
        constexpr auto structArrayToPtrArray(const RealMappingInfo (&structs)[N]) {
            std::array<const MappingInfo *, N> ret;
            for (size_t i = 0; i < N; ++i)
                ret[i] = &structs[i];
            return ret;
        }

        ''')

    for lang, lang_info in impl.languages.items():
        content += f'\nconstexpr RealMappingInfo g_{lang}Entries[] = {{\n'
        for variant in lang_info.variants:
            varname = variant.name
            variable = make_mapper_name(lang, varname)
            display = variant.display_name
            var_id = varname if varname != 'default' else ''
            content += f'    {{ L"{var_id}", L"{display}", {variable}<Range> }},\n'
        content += dedent(f'''\
            }};
            constexpr auto g_{lang} = structArrayToPtrArray(g_{lang}Entries);
            ''')
        
    
        
    content += dedent('''
        inline constexpr const ProfileInfo g_profiles[] = {
        ''')
    for idx, (lang, lang_info) in enumerate(impl.languages.items()):
        if idx > 0:
            content += ',\n'
        profileGuid = f'&g_guidProfile{lang.title()}'
        name = lang_info.name
        desc = f'{lang_info.name} Translit'
        icon = f'IDI_ICON_{lang.upper()}'
        compGuid = f'&g_guidCompartment{lang.title()}' if len(lang_info.variants) > 1 else 'nullptr'
        content += f'   {{{profileGuid}, L"{lang}", L"{name}", L"{desc}", {icon}, {compGuid}, g_{lang}}}'

    content += dedent('''
        };

        auto getProfiles() -> std::span<const ProfileInfo> {
            return g_profiles;
        }

        auto getProfileById(const GUID & profileId) -> const ProfileInfo * {
            for(auto & profile: g_profiles) {
                if (*profile.id == profileId)
                    return &profile;
            }
            return nullptr;
        }

        auto getMapper(const MappingInfo * info) -> Transliterator::MappingFunc * {
            return static_cast<const RealMappingInfo *>(info)->mapper;
        }

        ''')
    write_file_if_different(ROOTDIR / 'Translit/src/Languages.cpp', content)

def main():
    languages = ['be', 'he', 'ru', 'uk']
    for lang in languages:
        if not (ROOTDIR / f'Translit\\res\\{lang}.ico').exists():
            print(f'Icon {lang}.ico does not exist, please add', file=sys.stderr)
            return 1
        
    html = '<!-- THE TABLES BELOW ARE AUTO-GENERATED. DO NOT EDIT. -->\n'
    impl = Impl(headers=[], languages={})
    
    for language in languages:
        lang_file = f'{language}.toml'
        print(f'Processing {lang_file}')
        lang_config = MYDIR / lang_file
        with open(lang_config, 'rb') as f:
            config = tomllib.load(f)
        config['language'] = language

        generate_mapping_header(config)
        html += generate_div(config)
        generate_markdown(config)

        impl.headers.append(f'../tables/Table{language.upper()}.hpp')
        impl_lang = ImplLang(config['name'], [])
        for varname, varinfo in config['variants'].items():
            impl_lang.variants.append(ImplVar(varname, varinfo['display']))
        impl.languages[config['language']] = impl_lang

    generate_html(html)
    generate_implementation(impl)
    return 0
    

if __name__ == '__main__':
    sys.exit(main())

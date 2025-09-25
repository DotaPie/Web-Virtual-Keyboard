from pathlib import Path
import re
Import("env")

proj     = Path(env["PROJECT_DIR"])
web_dir  = proj / "web"
out_cpp  = proj / "src" / "html.cpp"
out_head = proj / "include" / "html.h"

def const_name(fname: str) -> str:
    base = fname.rsplit(".", 1)[0]
    name = re.sub(r'[^0-9a-zA-Z]+', "_", base).strip("_").upper()
    return f"{name}_HTML" if name else "HTML"

def delim_for(name: str) -> str:
    d = re.sub(r'[^A-Z0-9]', '', name)[:12] or "HTML"
    return f"{d}DL"

def generate_bundle():
    out_cpp.parent.mkdir(parents=True, exist_ok=True)
    out_head.parent.mkdir(parents=True, exist_ok=True)

    html_files = sorted(p for p in web_dir.glob("*.html") if p.is_file())

    if not html_files:
        h_body  = "#ifndef HTML_H\n#define HTML_H\n\n#include <pgmspace.h>\n\n#endif\n"
        cpp_body = '#include "html.h"\n\n// no html files\n'
    else:
        h_lines = [
            "#ifndef HTML_H",
            "#define HTML_H",
            "",
            "#include <pgmspace.h>",
            "",
        ]
        cpp_lines = ['#include "html.h"', ""]

        for p in html_files:
            cname = const_name(p.name)    # e.g. INDEX_HTML
            delim = delim_for(cname)      # e.g. INDEXHTMLDL
            raw   = p.read_text(encoding="utf-8")
            text = raw

            h_lines.append(f"extern const char {cname}[] PROGMEM;")
            # IMPORTANT: extern on the definition to give external linkage for const
            cpp_lines.append(f'extern const char {cname}[] PROGMEM = R"{delim}(')
            cpp_lines.append(text)
            cpp_lines.append(f'){delim}";\n')

        h_lines.append("\n#endif")
        h_body  = "\n".join(h_lines) + "\n"
        cpp_body = "\n".join(cpp_lines) + "\n"

    if not out_head.exists() or out_head.read_text(encoding="utf-8") != h_body:
        out_head.write_text(h_body, encoding="utf-8")
        print(f"[gen_web_bundle] wrote {out_head}")
    else:
        print(f"[gen_web_bundle] up to date: {out_head}")

    if not out_cpp.exists() or out_cpp.read_text(encoding="utf-8") != cpp_body:
        out_cpp.write_text(cpp_body, encoding="utf-8")
        print(f"[gen_web_bundle] wrote {out_cpp}")
    else:
        print(f"[gen_web_bundle] up to date: {out_cpp}")

# --- run now, so src/html.cpp exists before PlatformIO scans sources ---
generate_bundle()

# --- also re-generate before final link if sources changed ---
elf = "$BUILD_DIR/${PROGNAME}.elf"
def pre_link_action(*args, **kwargs):
    generate_bundle()
    return 0

env.AddPreAction(elf, pre_link_action)
# make the ELF depend on all html files so changes trigger the pre-action
html_nodes = env.Glob(str((web_dir / "*.html").as_posix()))
if html_nodes:
    env.Depends(elf, html_nodes)

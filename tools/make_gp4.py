#!/usr/bin/env python3
"""Generate a GP4 project and explicitly declare all parent directories."""
import argparse
import datetime
from xml.sax.saxutils import quoteattr

def tree(paths):
    root = {}
    for path in paths:
        node = root
        for part in path.replace("\\", "/").split("/")[:-1]:
            node = node.setdefault(part, {})
    return root

def emit(node, depth, out):
    for name in sorted(node):
        pad = "\t" * depth
        if node[name]:
            out.append(f"{pad}<dir targ_name={quoteattr(name)}>")
            emit(node[name], depth + 1, out)
            out.append(f"{pad}</dir>")
        else:
            out.append(f"{pad}<dir targ_name={quoteattr(name)} />")

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--out", required=True)
    p.add_argument("--content-id", required=True)
    p.add_argument("files", nargs="+")
    a = p.parse_args()
    files = [f.replace("\\", "/") for f in a.files]
    stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    x = ['<?xml version="1.0"?>',
         '<psproject xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" fmt="gp4" version="1000">',
         "\t<volume>", "\t\t<volume_type>pkg_ps4_app</volume_type>",
         "\t\t<volume_id>PS4VOLUME</volume_id>", f"\t\t<volume_ts>{stamp}</volume_ts>",
         f'\t\t<package content_id={quoteattr(a.content_id)} passcode="00000000000000000000000000000000" storage_type="digital50" app_type="full" />',
         '\t\t<chunk_info chunk_count="1" scenario_count="1">', "\t\t\t<chunks>",
         '\t\t\t\t<chunk id="0" layer_no="0" label="Chunk #0" />', "\t\t\t</chunks>",
         '\t\t\t<scenarios default_id="0">', '\t\t\t\t<scenario id="0" type="sp" initial_chunk_count="1" label="Scenario #0">0</scenario>',
         "\t\t\t</scenarios>", "\t\t</chunk_info>", "\t</volume>", '\t<files img_no="0">']
    for f in files:
        x.append(f"\t\t<file targ_path={quoteattr(f)} orig_path={quoteattr(f)} />")
    x += ["\t</files>", "\t<rootdir>"]
    emit(tree(files), 2, x)
    x += ["\t</rootdir>", "</psproject>"]
    with open(a.out, "w", newline="\n") as h:
        h.write("\n".join(x) + "\n")

if __name__ == "__main__":
    main()

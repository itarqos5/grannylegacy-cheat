import json

with open("flagged_methods.json") as f:
    data = json.load(f)

for cat, methods in data.items():
    print(f"\n=== {cat} ({len(methods)}) ===")
    for k, v in list(methods.items())[:15]:
        rva = v["rva"]
        offset = v["offset"]
        print(f"  {k:<60}  RVA: {rva}  Offset: {offset}")
    if len(methods) > 15:
        print(f"  ... and {len(methods) - 15} more (see flagged_methods.json)")
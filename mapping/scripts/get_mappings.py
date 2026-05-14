"""
get_mappings.py
---------------
Parses an IL2CPP dump.cs file and produces two JSON outputs:

  mappings.json         – flat { "ClassName.MethodName": "0xRVA" }
                          scoped to Assembly-CSharp.dll only
  flagged_methods.json  – methods grouped by game-mechanic category
                          { "Category": { "ClassName.MethodName":
                              { "rva": "...", "offset": "..." } } }
"""

import re
import json

# ---------------------------------------------------------------------------
# Assembly-CSharp type index range (from the dump header).
# Image 6: Assembly-CSharp.dll starts at TypeDefIndex 4633
# Image 7: System.Core.dll            starts at TypeDefIndex 4919
# ---------------------------------------------------------------------------
ASMCSHARP_START = 4633
ASMCSHARP_END   = 4919  # exclusive

# ---------------------------------------------------------------------------
# Flagging strategy: match on class name prefix OR method name keyword.
# Two separate tables keep things readable and precise.
# ---------------------------------------------------------------------------

# Classes whose *every* method is game-relevant (mapped to a category).
INTEREST_CLASSES: dict[str, str] = {
    # Player Health / Damage / Death
    "playerstatus":         "Player Health / Damage / Death",
    "gameovers":            "Player Health / Damage / Death",
    "ragdollpl":            "Player Health / Damage / Death",
    "p_hitgranny":          "Player Health / Damage / Death",
    "p_hitgrandpa":         "Player Health / Damage / Death",
    "grannyplankkill":      "Player Health / Damage / Death",
    "saunadeath":           "Player Health / Damage / Death",
    "poisoning":            "Player Health / Damage / Death",
    "trappoison":           "Player Health / Damage / Death",
    "freezing":             "Player Health / Damage / Death",
    # Movement Speed / Gravity
    "mobilefps":            "Movement Speed / Gravity",
    "crouchholder":         "Movement Speed / Gravity",
    "cameracontroller":     "Movement Speed / Gravity",
    "fallingholder":        "Movement Speed / Gravity",
    "sprintremovesitself":  "Movement Speed / Gravity",
    "windowjumping":        "Movement Speed / Gravity",
    # Item Count / Durability
    "handlepuzzles":        "Item Count / Durability",
    "pickray":              "Item Count / Durability",
    "itemspawn":            "Item Count / Durability",
    "weightcontroller":     "Item Count / Durability",
    "objectsmanager":       "Item Count / Durability",
    "noiseydrop":           "Item Count / Durability",
    "noisedropcheck":       "Item Count / Durability",
    "noisyplayerpresent":   "Item Count / Durability",
    "beartrapplogic":       "Item Count / Durability",
    "beartraprlogic":       "Item Count / Durability",
    "beartraplogic":        "Item Count / Durability",
    "charginganimhold":     "Item Count / Durability",
    # AI Detection
    "ai_granny":            "AI Detection",
    "ai_grandpa":           "AI Detection",
    "ai_momspider":         "AI Detection",
    "blindsensor_grandpa":  "AI Detection",
    "blindsensor_granny":   "AI Detection",
    "eyes_grandpa":         "AI Detection",
    "eyes_granny":          "AI Detection",
    "eyes_momspider":       "AI Detection",
    "spideraggro":          "AI Detection",
    "enemycontroller":      "AI Detection",
    "enemyfootstep":        "AI Detection",
    "carsensor":            "AI Detection",
}

# Method-name keywords (applied when the class is NOT in INTEREST_CLASSES).
# Matched against lower-cased method name only for precision.
INTEREST_METHODS: dict[str, str] = {
    # Player Health / Damage / Death
    "dead":         "Player Health / Damage / Death",
    "death":        "Player Health / Damage / Death",
    "die":          "Player Health / Damage / Death",
    "damage":       "Player Health / Damage / Death",
    "hurt":         "Player Health / Damage / Death",
    "health":       "Player Health / Damage / Death",
    "kill":         "Player Health / Damage / Death",
    "gameover":     "Player Health / Damage / Death",
    # Movement Speed / Gravity
    "speed":        "Movement Speed / Gravity",
    "gravity":      "Movement Speed / Gravity",
    "jump":         "Movement Speed / Gravity",
    "crouch":       "Movement Speed / Gravity",
    "sprint":       "Movement Speed / Gravity",
    "velocity":     "Movement Speed / Gravity",
    # Item Count / Durability
    "pickup":       "Item Count / Durability",
    "dropitem":     "Item Count / Durability",
    "additem":      "Item Count / Durability",
    "removeitem":   "Item Count / Durability",
    "durability":   "Item Count / Durability",
    "itemcount":    "Item Count / Durability",
    # AI Detection
    "sensor":       "AI Detection",
    "cansee":       "AI Detection",
    "detect":       "AI Detection",
    "alertlevel":   "AI Detection",
    "fieldofview":  "AI Detection",
}

# ---------------------------------------------------------------------------
# Regexes
# ---------------------------------------------------------------------------
RVA_RE    = re.compile(
    r'//\s*RVA:\s*(0x[0-9A-Fa-f]+)\s+Offset:\s*(0x[0-9A-Fa-f]+)'
)
# Matches class/struct/interface declaration lines with a TypeDefIndex comment
TYPEDEF_RE = re.compile(r'//\s*TypeDefIndex:\s*(\d+)')
CLASSNAME_RE = re.compile(
    r'\b(?:class|struct|interface)\s+([\w.<>]+)'
)
# Method signature line: access modifier(s) followed by return type and name
METHOD_RE = re.compile(
    r'^\s*(?:(?:public|private|protected|internal|static|virtual|override'
    r'|extern|abstract|sealed|unsafe|new|readonly)\s+)+'
    r'[\w\[\]<>,\*\?\s]+\s+([\w]+)\s*[(<]'
)


def is_game_method(class_name: str, method_name: str) -> str | None:
    """Return matching category name, or None if not interesting."""
    cls_low    = class_name.lower()
    method_low = method_name.lower()

    # 1. Entire class is interesting
    if cls_low in INTEREST_CLASSES:
        return INTEREST_CLASSES[cls_low]

    # 2. Method name contains an interesting keyword
    for kw, cat in INTEREST_METHODS.items():
        if kw in method_low:
            return cat

    return None


def parse_dump(file_path: str):
    all_methods: dict[str, dict] = {}    # "Class.Method" -> {rva, offset}
    flagged:     dict[str, dict] = {}    # category -> {key -> {rva, offset}}

    current_class     = None
    in_asmcsharp      = False
    pending_rva       = None
    pending_offset    = None

    with open(file_path, encoding="utf-8", errors="replace") as f:
        for line in f:
            # ---------------------------------------------------------------
            # Track Assembly-CSharp scope via TypeDefIndex
            # ---------------------------------------------------------------
            tdm = TYPEDEF_RE.search(line)
            if tdm:
                idx = int(tdm.group(1))
                in_asmcsharp = ASMCSHARP_START <= idx < ASMCSHARP_END
                if in_asmcsharp:
                    cm = CLASSNAME_RE.search(line)
                    current_class = cm.group(1) if cm else current_class
                pending_rva = None
                continue

            if not in_asmcsharp:
                continue

            stripped = line.strip()

            # ---------------------------------------------------------------
            # Class / struct / interface header (no TypeDefIndex on this line)
            # ---------------------------------------------------------------
            cm = CLASSNAME_RE.search(stripped)
            if cm and not stripped.startswith("//"):
                current_class = cm.group(1)
                pending_rva = None
                continue

            # ---------------------------------------------------------------
            # RVA comment line
            # ---------------------------------------------------------------
            rm = RVA_RE.search(stripped)
            if rm:
                pending_rva    = rm.group(1)
                pending_offset = rm.group(2)
                continue

            # ---------------------------------------------------------------
            # Method signature line (must follow an RVA line)
            # ---------------------------------------------------------------
            if pending_rva and current_class:
                mm = METHOD_RE.match(line)
                if mm:
                    method_name = mm.group(1)
                    # Skip constructors / compiler artefacts
                    if method_name not in (".ctor", ".cctor", "b__", "get_", "set_"):
                        key = f"{current_class}.{method_name}"
                        entry = {"rva": pending_rva, "offset": pending_offset}
                        all_methods[key] = entry

                        cat = is_game_method(current_class, method_name)
                        if cat:
                            flagged.setdefault(cat, {})[key] = entry

                pending_rva    = None
                pending_offset = None

    # -----------------------------------------------------------------------
    # Write outputs
    # -----------------------------------------------------------------------
    flat = {k: v["rva"] for k, v in all_methods.items()}
    with open("mappings.json", "w") as jf:
        json.dump(flat, jf, indent=2)

    with open("flagged_methods.json", "w") as jf:
        json.dump(flagged, jf, indent=2)

    print(f"Assembly-CSharp methods extracted : {len(all_methods)}")
    print(f"Flagged (game-mechanic) methods   : "
          f"{sum(len(v) for v in flagged.values())}")
    for cat, methods in flagged.items():
        print(f"  [{cat}]: {len(methods)}")


if __name__ == "__main__":
    parse_dump("mapping/dump.cs")
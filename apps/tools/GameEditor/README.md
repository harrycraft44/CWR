# Poseidon Level Editor (GameEditor)

A standalone, interactive Unity-style Level Editor for the Poseidon Engine. The editor allows content creators to spawn 3D objects, manipulate them in real-time, construct levels, and save/load level scenes.

---

## Features

- **3D Viewport Navigation**: Flycam controls in the viewport panel (Hold **Right-Click** and use **W/A/S/D** to move, hold **Shift** to boost speed).
- **Transform Gizmos**: Professional translation, rotation, and scaling arrows (integrated via **ImGuizmo**).
- **Transform Hotkeys**: Standard viewport hotkeys to switch gizmo modes:
  - `W` for Translation
  - `E` for Rotation
  - `R` for Scaling
- **Coordinate Space Toggles**: Switch between **Local** and **World** transform spaces.
- **Snapping**: Enable grid snapping to align objects precisely to unit grid increments.
- **Visual Aids**: Infinite 3D grid and coordinate axis helpers (Red: X, Green: Y, Blue: Z).
- **Asset Browser**: Scans the relative `models/` directory for engine-native `.p3d` files and provides a "Spawn" button to place them.
- **Hierarchy & Inspector**: View spawned level entities and modify their transforms directly.
- **Scene Serialization**: Save and load level layouts to/from `.edscene` level files.

---

## OBJ to P3D Importer Pipeline

The engine renders models using its proprietary binary **P3D (MLOD)** format. To import models from external modeling software (like Blender, Maya, or 3ds Max), convert them to P3D using the converter script:

```bash
python scripts/obj2p3d.py input.obj models/output.p3d
```

### Technical Specification of the MLOD Converter
The corrected exporter handles formatting details required by the engine's loader:
1. **MLOD Header**: Writes signature (`MLOD`), versions, and LOD count.
2. **SP3X Block**: Correctly writes signature and header size structure directly following the main header.
3. **Vertex, Normal, and Face Tables**: Maps OBJ vertices, normals, and UVs.
   - Automatically performs **fan triangulation** for complex polygons (vertices > 4).
   - Supports OBJ relative offsets (negative indices).
4. **TAGG Section**: Encodes metadata tag lists ending with a `#EndOfFile#` tag.
5. **LOD Resolution**: Places the float resolution bytes correctly at the very end of the TAGG section.

---

## Compilation

Build the editor target using CMake and your configured compiler:

```bash
# Build target GameEditor
cmake --build build/win-x64-clang-rwdi --target GameEditor
```

---

## Running the Editor

Launch the editor executable from the built distribution:

```bash
# Executable location
dist/x64-win-rwdi/GameEditor.exe
```

*Note: The editor scans the folder named `models` relative to its execution directory for available models. Ensure you create a `models/` directory adjacent to the executable containing your `.p3d` files.*

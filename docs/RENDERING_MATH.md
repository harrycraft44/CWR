# CWR Viewport Rendering Mathematics

This document provides a detailed breakdown of the mathematical equations, coordinate spaces, projection pipelines, clipping algorithms, and line drawing operations used to render the 3D world in the CWR viewport.

---

## 1. Coordinate Systems & Camera Math

CWR operates in a **Right-Handed Coordinate System** where:
* **$+X$** points to the **Right**.
* **$+Y$** points **Up** (World space vertical axis).
* **$+Z$** points **Forward** (into the screen / away from the viewer).

```
        +Y (Up)
         |
         |   +Z (Forward)
         |  /
         | /
         +--------- +X (Right)
```

### Flycam Rotation & Vector Derivation
The editor camera's orientation is defined by **Yaw** ($\theta$, rotation around $Y$) and **Pitch** ($\phi$, rotation around the horizontal axis $X$).
In [GameEditorApp.cpp](file:///c:/Users/harry/Documents/shit/CWR/apps/tools/GameEditor/GameEditorApp.cpp#L960-L978), these angles are translated into three orthogonal unit vectors representing the camera's local axes:

* **Forward Vector ($\vec{f}$):** Direct line-of-sight pointing into the scene:
  $$\vec{f} = \begin{pmatrix} \sin(\theta)\cos(\phi) \\ -\sin(\phi) \\ \cos(\theta)\cos(\phi) \end{pmatrix}$$
* **Right Vector ($\vec{r}$):** Perpendicular horizontal vector pointing directly to the right side of the lens:
  $$\vec{r} = \begin{pmatrix} \cos(\theta) \\ 0 \\ -\sin(\theta) \end{pmatrix}$$
* **Up Vector ($\vec{u}$):** Perpendicular vertical vector pointing straight up relative to the camera lens:
  $$\vec{u} = \vec{f} \times \vec{r} = \begin{pmatrix} f_y r_z - f_z r_y \\ f_z r_x - f_x r_z \\ f_x r_y - f_y r_x \end{pmatrix}$$

Using $\vec{f} \times \vec{r}$ (Forward crossed with Right) guarantees the correct upward-pointing vector in a right-handed coordinate system, preventing inverted face-winding orders and avoiding unwanted backface culling.

### Camera View Matrix (World-to-Camera Space)
A camera is positioned at a world coordinate $\vec{p} = (p_x, p_y, p_z)^T$.
The world-to-camera matrix transforms points from world space into camera-relative space (where the camera is at the origin looking down $+Z$).

1. **Orientation Inverse:** Since the rotation matrix $R$ is orthogonal (composed of orthogonal unit vectors $\vec{r}$, $\vec{u}$, $\vec{f}$), its inverse is simply its transpose:
   $$R^T = \begin{pmatrix} r_x & r_y & r_z \\ u_x & u_y & u_z \\ f_x & f_y & f_z \end{pmatrix}$$
2. **Translation Inverse:** Translates the camera position to the origin:
   $$T^{-1} = -\vec{p} = \begin{pmatrix} -p_x \\ -p_y \\ -p_z \end{pmatrix}$$
3. **Combined Inverse Scaled View Matrix ($V_{inv}$):** Computed in [Math3DT.cpp](file:///c:/Users/harry/Documents/shit/CWR/engine/Poseidon/Foundation/Math/Math3DT.cpp#L171-L186) by multiplying the orientation and translation:
   $$V_{inv} = R^T \cdot T^{-1}$$
   $$\vec{t}_{inv} = R^T \cdot (-\vec{p})$$
   $$V_{inv} = \begin{pmatrix} r_x & r_y & r_z & t_{inv, x} \\ u_x & u_y & u_z & t_{inv, y} \\ f_x & f_y & f_z & t_{inv, z} \\ 0 & 0 & 0 & 1 \end{pmatrix}$$

---

## 2. Aspect Ratios & Resizing Mechanics

To prevent stretching and squishing when the viewport size changes, the aspect settings must be dynamically recomputed.

```
+-----------------------------------------------------------+
| Main Window (io.DisplaySize)                              |
|                                                           |
|    +-----------------------------+                        |
|    | Viewport Panel              |                        |
|    |                             |                        |
|    |                             |                        |
|    |   leftFOV, topFOV           |                        |
|    |                             |                        |
|    +-----------------------------+                        |
|                                                           |
+-----------------------------------------------------------+
```

### Viewport Dimension Mapping
ImGui reports the current panel's available size and screenspace position:
* `viewportSize = ImGui::GetContentRegionAvail();`
* `contentMin = ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMin();`

These are mapped to normalized window fractions ($[0.0 .. 1.0]$) relative to `io.DisplaySize` to define the 3D rendering boundaries (`AspectSettings` world crop rect):
$$\text{left} = \frac{\text{contentMin.x}}{\text{DisplaySize.x}}, \quad \text{right} = \frac{\text{contentMin.x} + \text{viewportSize.x}}{\text{DisplaySize.x}}$$
$$\text{top} = \frac{\text{contentMin.y}}{\text{DisplaySize.y}}, \quad \text{bottom} = \frac{\text{contentMin.y} + \text{viewportSize.y}}{\text{DisplaySize.y}}$$

* **Division-by-Zero Guards:** To prevent matrix corruption on frame 1 (when `io.DisplaySize` can temporarily be 0), divisions are only performed when `DisplaySize` is strictly positive.
* **Safe Initialization:** All aspect settings fields are initialized to safe default metrics in [Engine.hpp](file:///c:/Users/harry/Documents/shit/CWR/engine/Poseidon/Graphics/Core/Engine.hpp#L154-L163) to prevent garbage floats at startup.

### Field of View (FOV) Adjustments
The vertical FOV ($\text{topFOV}$) is locked to `0.75f` (equivalent to standard vertical projection scale).
The horizontal FOV ($\text{leftFOV}$) is dynamically scaled by the viewport's aspect ratio to ensure uniform spatial sizing:
$$\text{leftFOV} = 0.75f \times \frac{\text{viewportSize.x}}{\text{viewportSize.y}}$$

These aspect values are combined with the user-adjustable FOV zoom factor ($FOV_{zoom}$, default `0.85f`) in [Camera.cpp](file:///c:/Users/harry/Documents/shit/CWR/engine/Poseidon/World/Scene/Camera/Camera.cpp#L27-L32) to generate the camera's viewing boundaries:
$$c_{left} = FOV_{zoom} \times \text{leftFOV}, \quad c_{top} = FOV_{zoom} \times \text{topFOV}$$

---

## 3. The Projection & Scale Matrices

CWR uses two separate projection matrices: one for the GPU hardware rasterizer (`_projectionNormal`) and one for the CPU software-projected drawing pipeline (`_projection`).

### Camera Rescale Matrices
Before projection is applied, the view space coordinates must be normalized. This is done using the scale matrices calculated in `Camera::Adjust`:
* **Scale Matrix ($\text{Scale}$):** Shrinks the $X$ and $Y$ coordinates by the camera boundaries to normalize them:
  $$\text{Scale} = \begin{pmatrix} 1/c_{left} & 0 & 0 & 0 \\ 0 & 1/c_{top} & 0 & 0 \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}$$
* **Inverse Scale Matrix ($\text{Scale}^{-1}$):**
  $$\text{Scale}^{-1} = \begin{pmatrix} c_{left} & 0 & 0 & 0 \\ 0 & c_{top} & 0 & 0 \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}$$

* **Scaled Inverse Transform ($V_{scaled\_inv}$):**
  This matrix maps world space directly into scaled view space:
  $$V_{scaled\_inv} = \text{Scale} \cdot V_{inv}$$

---

### GPU Projection Matrix (`_projectionNormal`)
The GPU projection matrix maps normalized view-space coordinates to homogeneous clip space. It handles depth distribution mapping:
$$q = \frac{Z_{far}}{Z_{far} - Z_{near}}$$

The matrix is structured as:
$$P_{normal} = \begin{pmatrix} 1/c_{left} & 0 & 0 & 0 \\ 0 & 1/c_{top} & 0 & 0 \\ 0 & 0 & q & -q \cdot Z_{near} \\ 0 & 0 & 1 & 0 \end{pmatrix}$$

* **D3D/GL Layout Conversion:** Direct3D row-major matrices are converted to column-major format in [EngineGL33_Shaders.cpp](file:///c:/Users/harry/Documents/shit/CWR/engine/PoseidonGL33/EngineGL33_Shaders.cpp#L895) before uploading to the GPU shaders. The GPU shader applies:
  $$\text{gl\_Position} = P_{normal} \times V_{inv} \times W_{orld} \times \vec{p}_{local}$$

---

### CPU Projection Matrix (`_projection`)
Software-projected elements (such as 3D lines, grid overlays, and GUI bounding boxes) do not go through the GPU vertex shader. Instead, they are projected to physical screen pixels directly on the CPU.

Let the physical viewport rect on screen be defined by origin $(x_0, y_0)$ and dimensions $(w, h)$ in physical pixels:
* $x_0 = \text{worldLeft} \times W_{window}$
* $y_0 = \text{worldTop} \times H_{window}$
* $w = (\text{worldRight} - \text{worldLeft}) \times W_{window}$
* $h = (\text{worldBottom} - \text{worldTop}) \times H_{window}$

The CPU projection matrix maps normalized view space directly to screenspace pixel coordinates:
$$P_{cpu} = \begin{pmatrix} w/2 & 0 & x_0 + w/2 & 0 \\ 0 & -h/2 & y_0 + h/2 & 0 \\ 0 & 0 & q & -q \cdot Z_{near} \\ 0 & 0 & 1 & 0 \end{pmatrix}$$

* **Y-Axis Inversion:** The $P_{cpu}(1, 1)$ element is negative ($-h/2$) because screenspace pixel coordinates go downwards (origin in top-left), while normalized view-space coordinates go upwards.

---

## 4. Software Perspective Transform

For software-projected points, [Math3DT.hpp](file:///c:/Users/harry/Documents/shit/CWR/engine/Poseidon/Foundation/Math/Math3DT.hpp#L20-L33) implements `SetPerspectiveProject` to transform a view-space vertex $\vec{v}_{view} = (x, y, z)^T$ to a 3D screenspace coordinate $\vec{p}_{screen}$:

1. **Perspective Divide ($1/Z$):**
   $$\text{oow} = \frac{1}{z}$$
2. **Screenspace Mapping Equations:**
   $$x_{screen} = \left(\frac{w}{2}\right) \times \left(\frac{x}{z}\right) + x_0 + \frac{w}{2}$$
   $$y_{screen} = \left(-\frac{h}{2}\right) \times \left(\frac{y}{z}\right) + y_0 + \frac{h}{2}$$
   $$z_{depth} = q - \frac{q \cdot Z_{near}}{z}$$

These screenspace coordinates $(x_{screen}, y_{screen})$ are then passed directly to the 2D orthographic renderer.

---

## 5. Frustum Clipping Algorithms

If a vertex moves behind the camera ($z \le 0$), the perspective divide divides by zero or a negative number. This mirrors the coordinates diagonally across the origin, creating giant stretched polygons (often appearing as star or bowtie shapes). To prevent this, geometry must be clipped before the perspective divide.

```
       \   Left Plane   /
        \              /
         \  FRUSTUM   /
          \          /
  ---------+--------+--------- Near Plane (Z = ClipNear)
            \      /
             \    /
              \  /  Behind Camera (Z < ClipNear)
               \/ Camera Location (0,0,0)
```

### Frustum Classification (`NeedsClippingInline`)
Every transformed view-space vertex $\vec{v}_{view} = (x, y, z)^T$ is classified in [TransLight.cpp](file:///c:/Users/harry/Documents/shit/CWR/engine/Poseidon/Graphics/Rendering/Lighting/TransLight.cpp#L1166-L1197) against the 6 frustum clipping planes:

$$\begin{aligned}
\text{ClipFront}  &: z < Z_{near} \\
\text{ClipBack}   &: z > Z_{far} \\
\text{ClipLeft}   &: z < -x \\
\text{ClipRight}  &: z < x \\
\text{ClipTop}    &: z < -y \\
\text{ClipBottom} &: z < y
\end{aligned}$$

If a vertex violates any of these planes, its index is marked with the corresponding bitflag.

### 3D Plane Intersection
When a polygon edge from vertex $\vec{a}$ (inside the frustum) to vertex $\vec{b}$ (outside the frustum) crosses a clipping plane defined by normal $\vec{n}$ and offset $d$, the crossing point parameter $t \in [0, 1]$ is computed:
$$t = \frac{\vec{a} \cdot \vec{n} + d}{(\vec{a} - \vec{b}) \cdot \vec{n}}$$

The clipped boundary vertex is created by linear interpolation in [ClipDraw.cpp](file:///c:/Users/harry/Documents/shit/CWR/engine/Poseidon/Graphics/Rendering/Draw/ClipDraw.cpp#L53-L81):
$$\vec{p}_{clipped} = \vec{a} \cdot (1 - t) + \vec{b} \cdot t$$
All vertex attributes (UV coordinates, vertex colors, and specular highlights) are interpolated using $t$ to ensure smooth gradients along the clipped edge.

---

## 6. Line Rendering Mathematics

Grid lines and axes are drawn on the CPU as 3D line segments. The GPU rasterizer only draws triangles, so the 2D screenspace lines must be converted into 2D quads before drawing.

### Screenspace Quad Construction
Given a screenspace line from $\vec{p}_0 = (x_0, y_0)$ to $\vec{p}_1 = (x_1, y_1)$ with desired pixel thickness $w$ (derived from `color.A8() * 0.1`):

```
       x0Side                x1Side
         +---------------------+
   p0    |  Line Direction ->  |    p1
   *=====*=====================*=====* (Center Segment)
         |                     |
         +---------------------+
       x0                    x1
```

1. **Direction and Perpendicular Vectors:**
   $$dx = x_1 - x_0, \quad dy = y_1 - y_0$$
   $$\text{invDSize} = \frac{1}{\sqrt{dx^2 + dy^2}}$$
   The perpendicular unit vector is:
   $$\vec{u}_{\perp} = \begin{pmatrix} pdx \\ pdy \end{pmatrix} = \begin{pmatrix} +dy \cdot \text{invDSize} \\ -dx \cdot \text{invDSize} \end{pmatrix}$$
2. **Offsetting Endpoints:**
   The endpoints are shifted by half the line thickness perpendicular to the line direction:
   $$\vec{p}_{0, shift} = \vec{p}_0 - \vec{u}_{\perp} \cdot \frac{w}{2}$$
   $$\vec{p}_{1, shift} = \vec{p}_1 - \vec{u}_{\perp} \cdot \frac{w}{2}$$
3. **Constructing the Quad Vertices:**
   $$\begin{aligned}
   v_0 &= \vec{p}_{0, shift} \\
   v_1 &= \vec{p}_{0, shift} + \vec{u}_{\perp} \cdot w \\
   v_2 &= \vec{p}_{1, shift} + \vec{u}_{\perp} \cdot w \\
   v_3 &= \vec{p}_{1, shift}
   \end{aligned}$$

These four vertices form a 2D quad of width $w$ pixels along the segment, which is then submitted to the 2D pipeline (`RM2DTris`).

### 2D Orthographic Shader Projection
The 2D shader uses the window's physical width and height ($\text{vpW}$, $\text{vpH}$) to map the screenspace quad vertices to NDC space $[-1, 1]$:
$$x_{ndc} = \left(\frac{x_{screen}}{\text{vpW}}\right) \times 2.0 - 1.0$$
$$y_{ndc} = 1.0 - \left(\frac{y_{screen}}{\text{vpH}}\right) \times 2.0$$

To bypass hardware perspective interpolation (which warps screenspace lines into hyperbolic shapes/fins when endpoints have different $W$ weights), the screenspace shader sets:
$$\text{gl\_Position} = \begin{pmatrix} x_{ndc} \\ y_{ndc} \\ z_{depth} \\ 1.0 \end{pmatrix}$$
Because the $W$ component is $1.0$, the GPU performs linear interpolation in screenspace, rendering the line with straight edges and constant thickness while preserving the depth-buffer checks.

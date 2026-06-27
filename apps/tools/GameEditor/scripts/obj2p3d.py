import sys
import struct
import os

def parse_obj(filepath):
    vertices = []
    normals = []
    uvs = []
    faces = []
    
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'): continue
            parts = line.split()
            if not parts: continue
            
            if parts[0] == 'v':
                vertices.append((float(parts[1]), float(parts[2]), float(parts[3])))
            elif parts[0] == 'vn':
                normals.append((float(parts[1]), float(parts[2]), float(parts[3])))
            elif parts[0] == 'vt':
                uvs.append((float(parts[1]), float(parts[2])))
            elif parts[0] == 'f':
                face_verts = []
                for p in parts[1:]:
                    subparts = p.split('/')
                    
                    # Vertex Index
                    v_val = int(subparts[0])
                    v_idx = len(vertices) + v_val if v_val < 0 else v_val - 1
                    
                    # UV Index
                    t_idx = -1
                    if len(subparts) > 1 and subparts[1]:
                        t_val = int(subparts[1])
                        t_idx = len(uvs) + t_val if t_val < 0 else t_val - 1
                        
                    # Normal Index
                    n_idx = -1
                    if len(subparts) > 2 and subparts[2]:
                        n_val = int(subparts[2])
                        n_idx = len(normals) + n_val if n_val < 0 else n_val - 1
                        
                    face_verts.append((v_idx, n_idx, t_idx))
                
                # Triangulate polygons with more than 4 vertices
                if len(face_verts) <= 4:
                    faces.append(face_verts)
                else:
                    for i in range(1, len(face_verts) - 1):
                        faces.append([face_verts[0], face_verts[i], face_verts[i+1]])
                
    return vertices, normals, uvs, faces

def write_mlod(filepath, vertices, normals, uvs, faces):
    with open(filepath, 'wb') as f:
        # MLOD Header
        f.write(b'MLOD')
        f.write(struct.pack('<B B H I', 1, 0, 0, 1)) # Version 1.0, 1 LOD
        
        # SP3X Section (Starts immediately after MLOD Header)
        f.write(b'SP3X')
        f.write(struct.pack('<i i i i i i', 28, 41, len(vertices), len(normals), len(faces), 0))
        
        # PointEx
        for v in vertices:
            f.write(struct.pack('<f f f i', v[0], v[1], v[2], 0))
            
        # NormalTable
        for n in normals:
            f.write(struct.pack('<f f f', n[0], n[1], n[2]))
            
        # FaceTable
        for face in faces:
            # texture (32 bytes)
            f.write(b'\x00' * 32)
            n_verts = len(face)
            f.write(struct.pack('<i', n_verts))
            for i in range(4):
                if i < n_verts:
                    v_idx, n_idx, t_idx = face[i]
                    if not (0 <= n_idx < len(normals)):
                        n_idx = 0
                    u = uvs[t_idx][0] if (0 <= t_idx < len(uvs)) else 0.0
                    v = uvs[t_idx][1] if (0 <= t_idx < len(uvs)) else 0.0
                    f.write(struct.pack('<i i f f', v_idx, n_idx, u, v))
                else:
                    f.write(struct.pack('<i i f f', 0, 0, 0.0, 0.0))
            f.write(struct.pack('<i', 0)) # flags
            
        # TAGG
        f.write(b'TAGG')
        # #EndOfFile#
        tag_name = b'#EndOfFile#'
        tag_name += b'\x00' * (64 - len(tag_name))
        f.write(tag_name)
        f.write(struct.pack('<i', 0)) # size 0
        
        # Resolution float (read after #EndOfFile# in readTAGGSection)
        f.write(struct.pack('<f', 1e13))

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python obj2p3d.py input.obj output.p3d")
        sys.exit(1)
        
    v, n, u, f = parse_obj(sys.argv[1])
    if not n:
        n = [(0.0, 1.0, 0.0)]
    write_mlod(sys.argv[2], v, n, u, f)
    print(f"Exported {sys.argv[2]} successfully!")

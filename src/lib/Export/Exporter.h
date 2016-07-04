#ifndef EXPORTER_H
#define EXPORTER_H

#include <vector>
#include <string>

#include "MeshComponents.h"
#include "MiscFunctions.h"

namespace voxel2tet
{
enum Exporter_FileTypes { FT_VTK, FT_Poly, FT_OFF, FT_OOFEM, FT_SIMPLE };

class Exporter
{
private:
public:
    Exporter();
    Exporter(std :: vector< TriangleType * > *Triangles, std :: vector< VertexType * > *Vertices, std :: vector< EdgeType * > *Edges, std :: vector< TetType * > *Tets);
    std :: vector< TriangleType * > *Triangles;
    std :: vector< VertexType * > *Vertices;
    std :: vector< EdgeType * > *Edges;
    std :: vector< TetType * > *Tets;
    virtual void WriteSurfaceData(std :: string Filename) = 0;
    virtual void WriteVolumeData(std :: string Filename) = 0;
};
}

#endif // EXPORTER_H

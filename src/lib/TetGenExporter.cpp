#include "TetGenExporter.h"
#include <iostream>
#include <fstream>

namespace voxel2tet
{

TetGenExporter::TetGenExporter()
{

}

TetGenExporter::TetGenExporter(std :: vector <TriangleType*> *Triangles, std :: vector <VertexType*> *Vertices, std :: vector <EdgeType*> *Edges):
    Exporter (Triangles, Vertices, Edges)
{
    LOG("Create TetGenExporter object\n",0);
}

void TetGenExporter::WriteData(std::string Filename)
{
    STATUS("Write TetGen file %s\n", Filename.c_str());
    std::ofstream TetGenFile;
    TetGenFile.open(Filename);

    TetGenFile << "# Node definitions\n";
    TetGenFile << this->Vertices->size() << "\t3\t0\t0\n";

    for (auto v: *this->Vertices) {
        TetGenFile << v->ID << "\t" << v->c[0]<< "\t" << v->c[1]<< "\t" << v->c[2] << "\n";
    }

    TetGenFile << "\n # Facet list\n";
    TetGenFile << this->Triangles->size() << "\t0\n";

    int j=0;

    for (auto t: *this->Triangles) {
        TetGenFile << 1 << "\n" << 3;
        std::array<int,3> VertexIDs;
        for (int i=0; i<3; i++) {
            VertexIDs[i] = t->Vertices[i]->ID;
            TetGenFile << "\t" << VertexIDs[i];
        }
        TetGenFile << "\n";
        j++;
    }


}

}
#include <algorithm>
#include <vector>
#include <iterator>

#include "Voxel2Tet.h"
#include "Importer.h"
#include "hdf5DataReader.h"

namespace voxel2tet
{

Voxel2Tet::Voxel2Tet(Options *Opt)
{
    this->Opt = Opt;
}

Voxel2Tet::~Voxel2Tet()
{
    for (unsigned int i=0; i<PhaseEdges.size(); i++) {
        delete this->PhaseEdges.at(i);
    }

    for (auto s: this->Surfaces) {
        delete s;
    }

    delete this->Mesh;

}

void Voxel2Tet::LoadFile(std::string Filename)
{
    STATUS ("Load file %s\n", Filename.c_str());
    hdf5DataReader *DataReader = new hdf5DataReader();
    DataReader->LoadFile(Filename);
    this->Imp = DataReader;

    if (!this->Opt->has_key("spring_constant")) {
        double cellspace[3];
        this->Imp->GiveSpacing(cellspace);

        Opt->AddDefaultMap("spring_const", std::to_string(cellspace[0]));
    }

}

void Voxel2Tet::FindSurfaces()
{

    STATUS("Find surfaces\n", 0);

    int dim[3];
    double spacing[3], origin[3];
    this->Imp->GiveDimensions(dim);
    this->Imp->GiveSpacing(spacing);
    this->Imp->GiveOrigin(origin);

    std::vector <double> signs = {1, -1};

    for (int i=0; i<dim[0]; i++) {
        for (int j=0; j<dim[1]; j++) {
            for (int k=0; k<dim[2]; k++) {

                // Specifies directions in which we will look for different materials
                std::vector <std::vector <double>> testdirections = {{1,0,0}, {0,1,0}, {0,0,1}};
                // If we the adjacent material is of other type, we will create a square by varying the coordinates marked '1' in vdirections
                std::vector <std::vector <double>> vdirections = {{0,1,1}, {1,0,1}, {1,1,0}};
                // vindex is the indices of the coordinates
                std::vector <std::vector <int>> vindex = {{1,2},{0,2},{0,1}};

                // If we are on a boundary, we need to check what is outside of that boundary
                if (i==0) {
                    testdirections.push_back({-1,0,0});
                    vdirections.push_back({0,1,1});
                    vindex.push_back({1,2});
                }
                if (j==0) {
                    testdirections.push_back({0,-1,0});
                    vdirections.push_back({1,0,1});
                    vindex.push_back({0,2});
                }
                if (k==0) {
                    testdirections.push_back({0,0,-1});
                    vdirections.push_back({1,1,0});
                    vindex.push_back({0,1});
                }

                int ThisPhase = this->Imp->GiveMaterialIDByIndex(i, j, k);
                int NeighboringPhase;
                bool SamePhase;

                // Check material in each direction
                for (unsigned int m=0; m<testdirections.size(); m++) {
                    int testi = testdirections.at(m).at(0) + i;
                    int testj = testdirections.at(m).at(1) + j;
                    int testk = testdirections.at(m).at(2) + k;

                    // If comparing inside the domain, simply compare
                    if ( (testi>=0) & (testj>=0) & (testk>=0) & (testi<dim[0]) & (testj<dim[1]) & (testk<dim[2])) {
                        NeighboringPhase = this->Imp->GiveMaterialIDByIndex(testi, testj, testk);
                        SamePhase = (ThisPhase == NeighboringPhase);
                    } else {
                        // If we are comparing with the outside, take into account that a we might have void (i.e. 0) in both voxels
                        if (ThisPhase!=0) {
                            SamePhase = false;
                            NeighboringPhase = this->Imp->GiveMaterialIDByIndex(testi, testj, testk);
                        } else {
                            // Void-to-void connection
                            SamePhase = true;
                            NeighboringPhase = ThisPhase;
                        }
                    }

                    if (!SamePhase) {   // Add surface. I.e. a square separating the two voxels

                        // Compute centre off square
                        double c[3];
                        c[0] = (double(i) + double(testdirections.at(m).at(0))/2.0) * spacing[0] + origin[0] + spacing[0]/2.0;
                        c[1] = (double(j) + double(testdirections.at(m).at(1))/2.0) * spacing[1] + origin[1] + spacing[1]/2.0;
                        c[2] = (double(k) + double(testdirections.at(m).at(2))/2.0) * spacing[2] + origin[2] + spacing[2]/2.0;

                        // Compute coordinate of corner point
                        double delta[3];
                        delta[0] = spacing[0]*vdirections.at(m)[0] / 2.0;
                        delta[1] = spacing[1]*vdirections.at(m)[1] / 2.0;
                        delta[2] = spacing[2]*vdirections.at(m)[2] / 2.0;

                        std::vector <int> VoxelIDs;

                        for (auto s1: signs) {
                            for (auto s2: signs) {
                                double newvertex[3];
                                newvertex[0] = c[0];
                                newvertex[1] = c[1];
                                newvertex[2] = c[2];

                                newvertex[vindex.at(m).at(0)] = newvertex[vindex.at(m).at(0)] + s1*delta[vindex.at(m)[0]];
                                newvertex[vindex.at(m).at(1)] = newvertex[vindex.at(m).at(1)] + s2*delta[vindex.at(m)[1]];

                                int id = Mesh->VertexOctreeRoot->AddVertex(newvertex[0], newvertex[1], newvertex[2]);
                                LOG ("Corner (id=%u) at (%f, %f, %f)\n", id, newvertex[0], newvertex[1], newvertex[2]);
                                VoxelIDs.push_back(id);
                            }
                        }
                        AddSurfaceSquare(VoxelIDs, {ThisPhase, NeighboringPhase}, NeighboringPhase);
                    }
                }
            }
        }
    }
}


void Voxel2Tet :: FindEdges()
{

    STATUS ("Find edges\n\tIdentify vertices shared by surfaces...\n", 0);

    // Sort vertex vectors on all surfaces
    for(auto surface: this->Surfaces) {
        std::sort(surface->Vertices.begin(), surface->Vertices.end());
    }

    std::vector <VertexType*> EdgeVertices;

    // Find all vertices that are shared among the surfaces. Since this is in 3D,
    // one vertex can be shared by several surfaces while not being an endpoint
    // of the edge. Thus, first find all shared vertices and then trace along x, y, z
    // to find the edge

    for (unsigned int s1=0; s1<this->Surfaces.size(); s1++) {
        Surface *surface1 = this->Surfaces.at(s1);
        std::vector <VertexType*> SurfaceEdgeVertices;
        for (unsigned int s2=0; s2<this->Surfaces.size(); s2++) {
            Surface *surface2 = this->Surfaces.at(s2);

            for (int i=0; i<2; i++) {
                if (s1==s2) break;
                bool SharedPhaseFound=false;
                std::vector <VertexType*> SharedVertices;
                int mat1 = surface1->Phases[i];
                for (int j=0; j<2; j++) {
                    int mat2 = surface2->Phases[j];
                    if (mat1==mat2) {

                        LOG("Surfaces %p and %p shares phase %i\n", surface1, surface2, mat1);

                        SharedPhaseFound = true;

                        std::set_intersection(surface1->Vertices.begin(), surface1->Vertices.end(),
                                              surface2->Vertices.begin(), surface2->Vertices.end(),
                                              back_inserter(SharedVertices));

                        // If both sets of vertices has an intersection, that intersection is an edge
                        if (SharedVertices.size()>0) {
                            LOG ("Surfaces intersects\n",0);
                            for (auto Vertex: SharedVertices) {
                                EdgeVertices.push_back(Vertex);
                                SurfaceEdgeVertices.push_back(Vertex);
                            }
                        }
                        break;
                    }
                }
                if (SharedPhaseFound) break;
            }

        }
        std::sort(SurfaceEdgeVertices.begin(), SurfaceEdgeVertices.end());
        SurfaceEdgeVertices.erase( std::unique(SurfaceEdgeVertices.begin(), SurfaceEdgeVertices.end()), SurfaceEdgeVertices.end());

        for (auto v: SurfaceEdgeVertices) {
            surface1->FixedVertices.push_back(v);
        }

    }

    STATUS ("\tTrace edges...\n", 0);

    std::sort(EdgeVertices.begin(), EdgeVertices.end());
    EdgeVertices.erase( std::unique(EdgeVertices.begin(), EdgeVertices.end()), EdgeVertices.end());

    // Trace edges by for each vertex in EdgeVertices:
    //  * Check if the neightbour is in EdgeVertices as well
    //    * If so, add the edge segment to edge separating the materials surounding the midpoint of edge segment

    // Used to check which material surround the midpoint. See FindSurfaces for explanation.
    std::vector <std::vector <double>> testdirections = {{1,0,0}, {0,1,0}, {0,0,1}};
    std::vector <std::vector <double>> vdirections = {{0,1,1}, {1,0,1}, {1,1,0}};
    std::vector <std::vector <int>> vindex = {{1,2},{0,2},{0,1}};
    std::vector <double> signs = {1, -1};

    double spacing[3];
    this->Imp->GiveSpacing(spacing);

    for (auto v: EdgeVertices) {
        for (int i=0; i<3; i++) {
            // Find neighbour
            double c[3];
            for (int j=0; j<3; j++) c[j] = v->c[j] + testdirections.at(i).at(j)*spacing[j];
            VertexType *Neighbour = this->Mesh->VertexOctreeRoot->FindVertexByCoords(c[0], c[1], c[2]);

            if (std::find(EdgeVertices.begin(), EdgeVertices.end(), Neighbour) != EdgeVertices.end()) {
                LOG ("Found Neightbour %p for %p\n", Neighbour, v);
                double cm[3];
                for (int j=0; j<3; j++) cm[j]=v->c[j] + testdirections.at(i).at(j)*spacing[j]/2;

                // Check phases surrounding cm
                double delta[3];
                delta[0] = spacing[0]*vdirections.at(i)[0] / 2.0;
                delta[1] = spacing[1]*vdirections.at(i)[1] / 2.0;
                delta[2] = spacing[2]*vdirections.at(i)[2] / 2.0;
                std::vector <int> Phases;

                for (auto s1: signs) {
                    for (auto s2: signs) {
                        double testpoint[3];
                        testpoint[0] = cm[0];
                        testpoint[1] = cm[1];
                        testpoint[2] = cm[2];

                        testpoint[vindex.at(i).at(0)] = testpoint[vindex.at(i).at(0)] + s1*delta[vindex.at(i)[0]];
                        testpoint[vindex.at(i).at(1)] = testpoint[vindex.at(i).at(1)] + s2*delta[vindex.at(i)[1]];

                        int matid = this->Imp->GiveMaterialIDByCoordinate(testpoint[0], testpoint[1], testpoint[2]);
                        Phases.push_back(matid);

                    }
                }

                // If 3 or 4 phases surrounds cm, this is a PhaseEdge
                std::sort(Phases.begin(), Phases.end());
                Phases.erase( std::unique(Phases.begin(), Phases.end()), Phases.end());

                if (Phases.size()>=3) {
                    LOG ("PhaseEdge found\n", 0);
                    AddPhaseEdge({v, Neighbour}, Phases);
                }

            }
        }
    }

    STATUS ("\tSort and fix non-connected edges...\n", 0);
    LOG("Fix and sort edges\n", 0);

    // Ensure that only have inner connected edges. I.e. max two vertices not connected to any other vertex on the edge
    unsigned int i=0;
    while (i<this->PhaseEdges.size()) {
        std::vector <PhaseEdge*> *FixedEdges = new std::vector <PhaseEdge*>;

        this->PhaseEdges.at(i)->SortAndFixBrokenEdge(FixedEdges);

        // Erase current PhaseEdge and replace it with the ones in FixedEdges
        this->PhaseEdges.erase(this->PhaseEdges.begin() + i);
        this->PhaseEdges.insert(this->PhaseEdges.begin() + i, FixedEdges->begin(), FixedEdges->end());

        i=i+FixedEdges->size();

        // Cleanup
        /*for (auto fe: *FixedEdges) {
            delete fe;
        }
        delete FixedEdges;*/

    }

    STATUS("\tSplit phase edges at shared points\n",0);
    // Split phase edges at shared points

    for (auto v: EdgeVertices) {

        unsigned int j=0;
        std::vector <int> PhaseEdgeIDs;

        while (j<this->PhaseEdges.size()) {
            std::vector<VertexType*> FlatList=this->PhaseEdges.at(j)->GetFlatListOfVertices();

            bool VertexFound = std::find(FlatList.begin(), FlatList.end(), v)!=FlatList.end();

            if (VertexFound) {
                PhaseEdgeIDs.push_back(j);
            }

            j++;
        }

        if (PhaseEdgeIDs.size()>1) {
            LOG("Vertex %p is a shared point\n", v);

            // First, split all PhaseEdges at v and store them in vector NewPhaseEdges
            std::vector<PhaseEdge*> NewPhaseEdges;

            for (unsigned int j=0; j<PhaseEdgeIDs.size(); j++) {
                PhaseEdge *pe = this->PhaseEdges.at(PhaseEdgeIDs.at(j));
                LOG("Split PhaseEdge %p at vertex\n", pe);
                pe->SplitAtVertex(v, &NewPhaseEdges);
            }

            // Then, remove old PhaseEdges
            std::sort(PhaseEdgeIDs.begin(), PhaseEdgeIDs.end());

            for (int j=PhaseEdgeIDs.size()-1; j>=0; j--) {
                this->PhaseEdges.erase(this->PhaseEdges.begin()+PhaseEdgeIDs.at(j));
            }

            // Finally, add new PhaseEdges
            for (auto pe: NewPhaseEdges) {
                this->PhaseEdges.push_back(pe);
            }
        }
    }

}

void Voxel2Tet :: SmoothEdges()
{
    STATUS("Smooth edges\n", 0);
    for (auto e: this->PhaseEdges) {
        e->Smooth();
    }
}

void Voxel2Tet :: SmoothSurfaces()
{
    STATUS("Smooth surfaces\n", 0);
    for (auto s: this->Surfaces) {
        s->Smooth();
    }
}

void Voxel2Tet :: AddPhaseEdge(std::vector<VertexType*> EdgeSegment, std::vector<int> Phases)
{
    // Ensure that Phases argument is unique
    std::sort(Phases.begin(), Phases.end());
    PhaseEdge* ThisPhaseEdge=NULL;
    Phases.erase( std::unique(Phases.begin(), Phases.end()), Phases.end());

    // Find PhaseEdge
    for (auto pe: this->PhaseEdges) {
        std::vector <int> PhaseDiff;
        std::set_difference(Phases.begin(), Phases.end(),
                              pe->Phases.begin(), pe->Phases.end(),
                              back_inserter(PhaseDiff));
        if (PhaseDiff.size()==0) {
            ThisPhaseEdge = pe;
            break;
        }
    }

    // If PhaseEdge does not exists, create it
    if (ThisPhaseEdge==NULL) {
        ThisPhaseEdge = new PhaseEdge(this->Opt);
        ThisPhaseEdge->Phases = Phases;
        this->PhaseEdges.push_back(ThisPhaseEdge);
    }

    ThisPhaseEdge->EdgeSegments.push_back({EdgeSegment.at(0), EdgeSegment.at(1)});

}

void Voxel2Tet :: AddSurfaceSquare(std::vector<int> VoxelIDs, std::vector<int> phases, int normalphase)
{
    // Check is surface exists
    Surface *ThisSurface = NULL;
    int SurfaceID;
    for (unsigned int i=0; i<this->Surfaces.size(); i++) {
        if ( ( (this->Surfaces.at(i)->Phases[0] == phases.at(0) ) & ( this->Surfaces.at(i)->Phases[1] == phases.at(1) ) ) |
             ( (this->Surfaces.at(i)->Phases[0] == phases.at(1) ) & ( this->Surfaces.at(i)->Phases[1] == phases.at(0) ) ) ) {
            ThisSurface = this->Surfaces.at(i);
            SurfaceID = i;
            break;
        }
    }

    // If not, create it and add it to the list
    if (ThisSurface==NULL) {
        ThisSurface = new Surface(phases.at(0), phases.at(1), this->Opt);
        this->Surfaces.push_back(ThisSurface);
        SurfaceID = this->Surfaces.size()-1;
    }

    // Create square (i.e. two triangles)
    TriangleType *triangle0, *triangle1;
    triangle0 = Mesh->AddTriangle({VoxelIDs.at(0), VoxelIDs.at(1), VoxelIDs.at(2)});
    triangle1 = Mesh->AddTriangle({VoxelIDs.at(1), VoxelIDs.at(3), VoxelIDs.at(2)});
    triangle0->InterfaceID = SurfaceID;
    triangle1->InterfaceID = SurfaceID;
    triangle0->PosNormalMatID = normalphase;
    triangle1->PosNormalMatID = normalphase;

    // Update surface
    ThisSurface->AddTriangle(triangle0);
    ThisSurface->AddTriangle(triangle0);
    for (int i=0; i<4; i++) {
        ThisSurface->AddVertex(this->Mesh->Vertices.at(VoxelIDs.at(i)));
    }

}

void Voxel2Tet::LoadData()
{
    STATUS ("Load data\n", 0);
    if (this->Opt->has_key("i")) {
        LoadFile(this->Opt->GiveStringValue("i"));
    }
    BoundingBoxType bb;
    double spacing[3];

    bb = this->Imp->GiveBoundingBox();
    this->Imp->GiveSpacing(spacing);

    for (int i=0; i<3; i++) {
        bb.maxvalues[i] = bb.maxvalues[i] + spacing[i];
        bb.minvalues[i] = bb.minvalues[i] - spacing[i];
    }
    Mesh = new MeshData(bb);
}

void Voxel2Tet::Process()
{
    STATUS ("Proccess content\n", 0);

    this->FindSurfaces();

    this->FindEdges();

    this->SmoothEdges();

    this->SmoothSurfaces();

}

}

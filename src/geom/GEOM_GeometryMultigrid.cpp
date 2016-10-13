




#include "GEOM_GeometryMultigrid.hpp"
#include "GEOM_GeometryMultigridQueue.hpp"

namespace ARIES
{
    namespace GEOM
    {
        GEOM_GeometryMultigrid::GEOM_GeometryMultigrid(GEOM_Geometry ***geometry, TBOX::TBOX_Config **config_container, unsigned short iMesh, unsigned short iZone) : GEOM_Geometry()
        {
            /*--- GEOM_Geometry & TBOX::TBOX_Config pointers to the fine grid level for clarity. We may
            need access to the other zones in the mesh for zone boundaries. ---*/
            GEOM_Geometry *fine_grid = geometry[iZone][iMesh - 1];
            TBOX::TBOX_Config *config = config_container[iZone];

            /*--- Local variables ---*/
            unsigned long iPoint, Index_CoarseCV, CVPoint, iElem, iVertex, jPoint, iteration, nVertexS, nVertexR, nBufferS_vector, nBufferR_vector, iParent, jVertex, *Buffer_Receive_Parent = NULL, *Buffer_Send_Parent = NULL, *Buffer_Receive_Children = NULL, *Buffer_Send_Children = NULL, *Parent_Remote = NULL, *Children_Remote = NULL, *Parent_Local = NULL, *Children_Local = NULL, Local_nPointCoarse, Local_nPointFine, Global_nPointCoarse, Global_nPointFine;;
            short marker_seed;
            bool agglomerate_seed = true;
            unsigned short nChildren, iNode, counter, iMarker, jMarker, priority, MarkerS, MarkerR, *nChildren_MPI;
            std::vector<unsigned long> Suitable_Indirect_Neighbors, Aux_Parent;
            std::vector<unsigned long>::iterator it;
            int rank;

            unsigned short nMarker_Max = config->GetnMarker_Max();

            unsigned short *copy_marker = new unsigned short[nMarker_Max];

#ifndef HAVE_MPI
            rank = TBOX::MASTER_NODE;
#else
            int send_to, receive_from;
            MPI_Comm_rank(MPI_COMM_WORLD, &rank);
            MPI_Status status;
#endif

            nDim = fine_grid->GetnDim(); // Write the number of dimensions of the coarse grid.

            /*--- Create a queue system to deo the agglomeration
            1st) More than two markers ---> Vertices (never agglomerate)
            2nd) Two markers ---> Edges (agglomerate if same BC, never agglomerate if different BC)
            3rd) One marker ---> Surface (always agglomarate)
            4th) No marker ---> Internal Volume (always agglomarate) ---*/

            /*--- Set a marker to indicate indirect agglomeration ---*/
            if (iMesh == TBOX::MESH_1)
            {

                for (iPoint = 0; iPoint < fine_grid->GetnPoint(); iPoint++)
                    fine_grid->node[iPoint]->SetAgglomerate_Indirect(false);

                for (iElem = 0; iElem < fine_grid->GetnElem(); iElem++)
                {
                    if ((fine_grid->elem[iElem]->GetVTK_Type() == TBOX::HEXAHEDRON) ||
                        (fine_grid->elem[iElem]->GetVTK_Type() == TBOX::RECTANGLE))
                    {
                        for (iNode = 0; iNode < fine_grid->elem[iElem]->GetnNodes(); iNode++)
                        {
                            iPoint = fine_grid->elem[iElem]->GetNode(iNode);
                            fine_grid->node[iPoint]->SetAgglomerate_Indirect(true);
                        }
                    }
                }
            }

            /*--- Create the coarse grid structure using as baseline the fine grid ---*/
            GEOM_GeometryMultigridQueue MGQueue_InnerCV(fine_grid->GetnPoint());

            node = new GRID::GRID_DGPoint*[fine_grid->GetnPoint()];
            for (iPoint = 0; iPoint < fine_grid->GetnPoint(); iPoint++) 
            {

                /*--- Create node structure ---*/

                node[iPoint] = new GRID::GRID_DGPoint(nDim, iPoint, config);

                /*--- Set the indirect agglomeration to false ---*/

                node[iPoint]->SetAgglomerate_Indirect(false);
            }

            Index_CoarseCV = 0;

            /*--- The first step is the boundary agglomeration. ---*/
            for (iMarker = 0; iMarker < fine_grid->GetnMarker(); iMarker++)
            {
                for (iVertex = 0; iVertex < fine_grid->GetnVertex(iMarker); iVertex++)
                {
                    iPoint = fine_grid->vertex[iMarker][iVertex]->GetNode();

                    /*--- If the element has not being previously agglomerated and it belongs
                    to the physical domain, then the agglomeration is studied ---*/

                    if ((fine_grid->node[iPoint]->GetAgglomerate() == false) &&
                        (fine_grid->node[iPoint]->GetDomain()) &&
                        (GeometricalCheck(iPoint, fine_grid, config)))
                    {

                        nChildren = 1;

                        /*--- We set an index for the parent control volume ---*/
                        fine_grid->node[iPoint]->SetParent_CV(Index_CoarseCV);

                        /*--- We add the seed point (child) to the parent control volume ---*/
                        node[Index_CoarseCV]->SetChildren_CV(0, iPoint);
                        agglomerate_seed = true; counter = 0; marker_seed = iMarker;

                        /*--- For a particular point in the fine grid we save all the markers
                        that are in that point ---*/

                        for (jMarker = 0; jMarker < fine_grid->GetnMarker(); jMarker++)
                            if (fine_grid->node[iPoint]->GetVertex(jMarker) != -1)
                            {
                                copy_marker[counter] = jMarker;
                                counter++;
                            }

                        /*--- To aglomerate a vertex it must have only one physical bc!!
                        This can be improved. If there is only a marker, it is a good
                        candidate for agglomeration ---*/
                        if (counter == 1) agglomerate_seed = true;

                        /*--- If there are two markers, we will aglomerate if one of the
                        marker is SEND_RECEIVE ---*/

                        if (counter == 2)
                        {
                            if ((config->GetMarker_All_KindBC(copy_marker[0]) == TBOX::SEND_RECEIVE) ||
                                (config->GetMarker_All_KindBC(copy_marker[1]) == TBOX::SEND_RECEIVE)) agglomerate_seed = true;
                            else agglomerate_seed = false;
                        }

                        /*--- If there are more than 2 markers, the aglomeration will be discarted ---*/

                        if (counter > 2) agglomerate_seed = false;

                        /*--- If the seed can be agglomerated, we try to agglomerate more points ---*/

                        if (agglomerate_seed)
                        {

                            /*--- Now we do a sweep over all the nodes that surround the seed point ---*/

                            for (iNode = 0; iNode < fine_grid->node[iPoint]->GetnPoint(); iNode++)
                            {

                                CVPoint = fine_grid->node[iPoint]->GetPoint(iNode);

                                /*--- The new point can be agglomerated ---*/

                                if (SetBoundAgglomeration(CVPoint, marker_seed, fine_grid, config))
                                {

                                    /*--- We set the value of the parent ---*/

                                    fine_grid->node[CVPoint]->SetParent_CV(Index_CoarseCV);

                                    /*--- We set the value of the child ---*/

                                    node[Index_CoarseCV]->SetChildren_CV(nChildren, CVPoint);
                                    nChildren++;
                                }

                            }

                            Suitable_Indirect_Neighbors.clear();

                            if (fine_grid->node[iPoint]->GetAgglomerate_Indirect())
                                SetSuitableNeighbors(&Suitable_Indirect_Neighbors, iPoint, Index_CoarseCV, fine_grid);

                            /*--- Now we do a sweep over all the indirect nodes that can be added ---*/
                            for (iNode = 0; iNode < Suitable_Indirect_Neighbors.size(); iNode++)
                            {

                                CVPoint = Suitable_Indirect_Neighbors[iNode];

                                /*--- The new point can be agglomerated ---*/
                                if (SetBoundAgglomeration(CVPoint, marker_seed, fine_grid, config))
                                {

                                    /*--- We set the value of the parent ---*/
                                    fine_grid->node[CVPoint]->SetParent_CV(Index_CoarseCV);

                                    /*--- We set the indirect agglomeration information ---*/
                                    if (fine_grid->node[CVPoint]->GetAgglomerate_Indirect())
                                        node[Index_CoarseCV]->SetAgglomerate_Indirect(true);

                                    /*--- We set the value of the child ---*/
                                    node[Index_CoarseCV]->SetChildren_CV(nChildren, CVPoint);
                                    nChildren++;
                                }
                            }
                        }

                        /*--- Update the number of child of the control volume ---*/

                        node[Index_CoarseCV]->SetnChildren_CV(nChildren);
                        Index_CoarseCV++;
                    }
                }
            }

            /*--- Agglomerate all the nodes that have more than one physical boundary condition,
            Maybe here we can add the posibility of merging the vertex that have the same number,
            and kind  of markers---*/

            for (iMarker = 0; iMarker < fine_grid->GetnMarker(); iMarker++)
                for (iVertex = 0; iVertex < fine_grid->GetnVertex(iMarker); iVertex++)
                {
                    iPoint = fine_grid->vertex[iMarker][iVertex]->GetNode();
                    if ((fine_grid->node[iPoint]->GetAgglomerate() == false) &&
                        (fine_grid->node[iPoint]->GetDomain()))
                    {
                        fine_grid->node[iPoint]->SetParent_CV(Index_CoarseCV);
                        node[Index_CoarseCV]->SetChildren_CV(0, iPoint);
                        node[Index_CoarseCV]->SetnChildren_CV(1);
                        Index_CoarseCV++;
                    }
                }

            /*--- Update the queue with the results from the boundary agglomeration ---*/
            for (iPoint = 0; iPoint < fine_grid->GetnPoint(); iPoint++)
            {
                /*--- The CV has been agglomerated, remove form the list ---*/
                if (fine_grid->node[iPoint]->GetAgglomerate() == true)
                {
                    MGQueue_InnerCV.RemoveCV(iPoint);
                }

                else
                {

                    /*--- Count the number of agglomerated neighbors, and modify the queue ---*/
                    priority = 0;
                    for (iNode = 0; iNode < fine_grid->node[iPoint]->GetnPoint(); iNode++)
                    {
                        jPoint = fine_grid->node[iPoint]->GetPoint(iNode);
                        if (fine_grid->node[jPoint]->GetAgglomerate() == true) priority++;
                    }
                    MGQueue_InnerCV.MoveCV(iPoint, priority);
                }
            }

            /*--- Agglomerate the domain nodes ---*/
            iteration = 0;
            while (!MGQueue_InnerCV.EmptyQueue() && (iteration < fine_grid->GetnPoint()))
            {
                iPoint = MGQueue_InnerCV.NextCV();
                iteration++;

                /*--- If the element has not being previously agglomerated, belongs to the physical domain,
                and satisfies several geometrical criteria then the seed CV is acepted for agglomeration ---*/

                if ((fine_grid->node[iPoint]->GetAgglomerate() == false) &&
                    (fine_grid->node[iPoint]->GetDomain()) &&
                    (GeometricalCheck(iPoint, fine_grid, config)))
                {

                    nChildren = 1;

                    /*--- We set an index for the parent control volume ---*/
                    fine_grid->node[iPoint]->SetParent_CV(Index_CoarseCV);

                    /*--- We add the seed point (child) to the parent control volume ---*/
                    node[Index_CoarseCV]->SetChildren_CV(0, iPoint);

                    /*--- Update the queue with the seed point (remove the seed and
                    increase the priority of the neighbors) ---*/
                    MGQueue_InnerCV.Update(iPoint, fine_grid);

                    /*--- Now we do a sweep over all the nodes that surround the seed point ---*/
                    for (iNode = 0; iNode < fine_grid->node[iPoint]->GetnPoint(); iNode++)
                    {

                        CVPoint = fine_grid->node[iPoint]->GetPoint(iNode);

                        /*--- Determine if the CVPoint can be agglomerated ---*/
                        if ((fine_grid->node[CVPoint]->GetAgglomerate() == false) &&
                            (fine_grid->node[CVPoint]->GetDomain()) &&
                            (GeometricalCheck(CVPoint, fine_grid, config)))
                        {

                            /*--- We set the value of the parent ---*/
                            fine_grid->node[CVPoint]->SetParent_CV(Index_CoarseCV);

                            /*--- We set the value of the child ---*/
                            node[Index_CoarseCV]->SetChildren_CV(nChildren, CVPoint);
                            nChildren++;

                            /*--- Update the queue with the new control volume (remove the CV and
                            increase the priority of the neighbors) ---*/
                            MGQueue_InnerCV.Update(CVPoint, fine_grid);
                        }
                    }

                    /*--- Subrotuine to identify the indirect neighbors ---*/
                    Suitable_Indirect_Neighbors.clear();
                    if (fine_grid->node[iPoint]->GetAgglomerate_Indirect())
                        SetSuitableNeighbors(&Suitable_Indirect_Neighbors, iPoint, Index_CoarseCV, fine_grid);

                    /*--- Now we do a sweep over all the indirect nodes that can be added ---*/

                    for (iNode = 0; iNode < Suitable_Indirect_Neighbors.size(); iNode++)
                    {

                        CVPoint = Suitable_Indirect_Neighbors[iNode];

                        /*--- The new point can be agglomerated ---*/

                        if ((fine_grid->node[CVPoint]->GetAgglomerate() == false) &&
                            (fine_grid->node[CVPoint]->GetDomain()))
                        {

                            /*--- We set the value of the parent ---*/

                            fine_grid->node[CVPoint]->SetParent_CV(Index_CoarseCV);

                            /*--- We set the indirect agglomeration information ---*/

                            if (fine_grid->node[CVPoint]->GetAgglomerate_Indirect())
                                node[Index_CoarseCV]->SetAgglomerate_Indirect(true);

                            /*--- We set the value of the child ---*/

                            node[Index_CoarseCV]->SetChildren_CV(nChildren, CVPoint);
                            nChildren++;

                            /*--- Update the queue with the new control volume (remove the CV and
                            increase the priority of the neighbors) ---*/

                            MGQueue_InnerCV.Update(CVPoint, fine_grid);

                        }
                    }

                    /*--- Update the number of control of childrens ---*/

                    node[Index_CoarseCV]->SetnChildren_CV(nChildren);
                    Index_CoarseCV++;
                }
                else
                {

                    /*--- The seed point can not be agglomerated because of size, domain, streching, etc.
                    move the point to the lowest priority ---*/

                    MGQueue_InnerCV.MoveCV(iPoint, -1);
                }

            }

            /*--- Add all the elements that have not being agglomerated, in the previous stage ---*/

            for (iPoint = 0; iPoint < fine_grid->GetnPoint(); iPoint++)
            {
                if ((fine_grid->node[iPoint]->GetAgglomerate() == false) && (fine_grid->node[iPoint]->GetDomain()))
                {

                    nChildren = 1;
                    fine_grid->node[iPoint]->SetParent_CV(Index_CoarseCV);
                    if (fine_grid->node[iPoint]->GetAgglomerate_Indirect())
                        node[Index_CoarseCV]->SetAgglomerate_Indirect(true);
                    node[Index_CoarseCV]->SetChildren_CV(0, iPoint);
                    node[Index_CoarseCV]->SetnChildren_CV(nChildren);
                    Index_CoarseCV++;

                }
            }

            nPointDomain = Index_CoarseCV;

            /*--- Check that there are no hanging nodes ---*/

            unsigned long iFinePoint, iFinePoint_Neighbor, iCoarsePoint, iCoarsePoint_Complete;
            unsigned short iChildren;

            /*--- Find the point surrounding a point ---*/

            for (iCoarsePoint = 0; iCoarsePoint < nPointDomain; iCoarsePoint++)
            {
                for (iChildren = 0; iChildren < node[iCoarsePoint]->GetnChildren_CV(); iChildren++)
                {
                    iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
                    for (iNode = 0; iNode < fine_grid->node[iFinePoint]->GetnPoint(); iNode++)
                    {
                        iFinePoint_Neighbor = fine_grid->node[iFinePoint]->GetPoint(iNode);
                        iParent = fine_grid->node[iFinePoint_Neighbor]->GetParent_CV();
                        if (iParent != iCoarsePoint) node[iCoarsePoint]->SetPoint(iParent);
                    }
                }
            }

            /*--- Detect isolated points and merge them with its correct neighbor ---*/

            for (iCoarsePoint = 0; iCoarsePoint < nPointDomain; iCoarsePoint++)
            {

                if (node[iCoarsePoint]->GetnPoint() == 1)
                {

                    /*--- Find the neighbor of the isolated point. This neighbor is the right control volume ---*/

                    iCoarsePoint_Complete = node[iCoarsePoint]->GetPoint(0);

                    /*--- Add the children to the connected control volume (and modify it parent indexing).
                    Identify the child CV from the finest grid and added to the correct control volume.
                    Set the parent CV of iFinePoint. Instead of using the original
                    (iCoarsePoint) one use the new one (iCoarsePoint_Complete) ---*/

                    nChildren = node[iCoarsePoint_Complete]->GetnChildren_CV();

                    for (iChildren = 0; iChildren < node[iCoarsePoint]->GetnChildren_CV(); iChildren++)
                    {
                        iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
                        node[iCoarsePoint_Complete]->SetChildren_CV(nChildren, iFinePoint);
                        nChildren++;
                        fine_grid->node[iFinePoint]->SetParent_CV(iCoarsePoint_Complete);
                    }

                    /*--- Update the number of children control volumes ---*/

                    node[iCoarsePoint_Complete]->SetnChildren_CV(nChildren);
                    node[iCoarsePoint]->SetnChildren_CV(0);

                }
            }

            //  unsigned long iPointFree = nPointDomain-1;
            //  iCoarsePoint = 0;
            //
            //  do {
            //
            //    if (node[iCoarsePoint]->GetnChildren_CV() == 0) {
            //
            //      while (node[iPointFree]->GetnChildren_CV() == 0) {
            //        Index_CoarseCV--;
            //        iPointFree--;
            //      }
            //
            //      nChildren = node[iPointFree]->GetnChildren_CV();
            //      for (iChildren = 0; iChildren <  nChildren; iChildren ++) {
            //        iFinePoint = node[iPointFree]->GetChildren_CV(iChildren);
            //        node[iCoarsePoint]->SetChildren_CV(iChildren, iFinePoint);
            //        fine_grid->node[iFinePoint]->SetParent_CV(iCoarsePoint);
            //      }
            //      node[iCoarsePoint]->SetnChildren_CV(nChildren);
            //      node[iPointFree]->SetnChildren_CV(0);
            //
            //      Index_CoarseCV--;
            //      iPointFree--;
            //
            //    }
            //
            //    iCoarsePoint++;
            //
            //  } while ((iCoarsePoint-1) < Index_CoarseCV);
            //
            //  nPointDomain = Index_CoarseCV;

            /*--- Reset the point surrounding a point ---*/

            for (iCoarsePoint = 0; iCoarsePoint < nPointDomain; iCoarsePoint++)
            {
                node[iCoarsePoint]->ResetPoint();
            }

            /*--- Dealing with MPI parallelization, the objective is that the received nodes must be agglomerated
            in the same way as the donor nodes. Send the node agglomeration information of the donor
            (parent and children), Sending only occurs with MPI ---*/

            for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
            {
                if ((config->GetMarker_All_KindBC(iMarker) == TBOX::SEND_RECEIVE) &&
                    (config->GetMarker_All_SendRecv(iMarker) > 0))
                {
                    MarkerS = iMarker;  MarkerR = iMarker + 1;

#ifdef HAVE_MPI
                    send_to = config->GetMarker_All_SendRecv(MarkerS) - 1;
                    receive_from = abs(config->GetMarker_All_SendRecv(MarkerR)) - 1;
#endif

                    nVertexS = fine_grid->nVertex[MarkerS];   nVertexR = fine_grid->nVertex[MarkerR];
                    nBufferS_vector = nVertexS;               nBufferR_vector = nVertexR;

                    /*--- Allocate Receive and send buffers  ---*/

                    Buffer_Receive_Children = new unsigned long[nBufferR_vector];
                    Buffer_Send_Children = new unsigned long[nBufferS_vector];

                    Buffer_Receive_Parent = new unsigned long[nBufferR_vector];
                    Buffer_Send_Parent = new unsigned long[nBufferS_vector];

                    /*--- Copy the information that should be sended ---*/

                    for (iVertex = 0; iVertex < nVertexS; iVertex++)
                    {
                        iPoint = fine_grid->vertex[MarkerS][iVertex]->GetNode();
                        Buffer_Send_Children[iVertex] = iPoint;
                        Buffer_Send_Parent[iVertex] = fine_grid->node[iPoint]->GetParent_CV();
                    }

#ifdef HAVE_MPI
                    /*--- Send/Receive information using Sendrecv ---*/
                    MPI_Sendrecv(Buffer_Send_Children, nBufferS_std::vector, MPI_UNSIGNED_LONG, send_to, 0,
                        Buffer_Receive_Children, nBufferR_std::vector, MPI_UNSIGNED_LONG, receive_from, 0, MPI_COMM_WORLD, &status);
                    MPI_Sendrecv(Buffer_Send_Parent, nBufferS_std::vector, MPI_UNSIGNED_LONG, send_to, 1,
                        Buffer_Receive_Parent, nBufferR_std::vector, MPI_UNSIGNED_LONG, receive_from, 1, MPI_COMM_WORLD, &status);
#else
                    /*--- Receive information without MPI ---*/
                    for (iVertex = 0; iVertex < nVertexR; iVertex++)
                    {
                        Buffer_Receive_Children[iVertex] = Buffer_Send_Children[iVertex];
                        Buffer_Receive_Parent[iVertex] = Buffer_Send_Parent[iVertex];
                    }
#endif

                    /*--- Deallocate send buffer ---*/

                    delete[] Buffer_Send_Children;
                    delete[] Buffer_Send_Parent;

                    /*--- Create a list of the parent nodes without repeated parents ---*/

                    Aux_Parent.clear();
                    for (iVertex = 0; iVertex < nVertexR; iVertex++)
                        Aux_Parent.push_back(Buffer_Receive_Parent[iVertex]);

                    sort(Aux_Parent.begin(), Aux_Parent.end());
                    it = unique(Aux_Parent.begin(), Aux_Parent.end());
                    Aux_Parent.resize(it - Aux_Parent.begin());

                    /*--- Allocate some structures ---*/

                    Parent_Remote = new unsigned long[nVertexR];
                    Children_Remote = new unsigned long[nVertexR];
                    Parent_Local = new unsigned long[nVertexR];
                    Children_Local = new unsigned long[nVertexR];

                    /*--- Create the local std::vector and remote for the parents and the children ---*/

                    for (iVertex = 0; iVertex < nVertexR; iVertex++) {

                        Parent_Remote[iVertex] = Buffer_Receive_Parent[iVertex];

                        /*--- We use the same sorting as in the donor domain ---*/

                        for (jVertex = 0; jVertex < Aux_Parent.size(); jVertex++)
                        {
                            if (Parent_Remote[iVertex] == Aux_Parent[jVertex])
                            {
                                Parent_Local[iVertex] = jVertex + Index_CoarseCV;
                                break;
                            }
                        }

                        Children_Remote[iVertex] = Buffer_Receive_Children[iVertex];
                        Children_Local[iVertex] = fine_grid->vertex[MarkerR][iVertex]->GetNode();

                    }

                    Index_CoarseCV += Aux_Parent.size();

                    nChildren_MPI = new unsigned short[Index_CoarseCV];
                    for (iParent = 0; iParent < Index_CoarseCV; iParent++)
                        nChildren_MPI[iParent] = 0;

                    /*--- Create the final structure ---*/
                    for (iVertex = 0; iVertex < nVertexR; iVertex++)
                    {
                        /*--- Be careful, it is possible that a node change the agglomeration configuration, the priority
                        is always, when receive the information ---*/
                        fine_grid->node[Children_Local[iVertex]]->SetParent_CV(Parent_Local[iVertex]);
                        node[Parent_Local[iVertex]]->SetChildren_CV(nChildren_MPI[Parent_Local[iVertex]], Children_Local[iVertex]);
                        nChildren_MPI[Parent_Local[iVertex]]++;
                        node[Parent_Local[iVertex]]->SetnChildren_CV(nChildren_MPI[Parent_Local[iVertex]]);
                        node[Parent_Local[iVertex]]->SetDomain(false);
                    }

                    /*--- Deallocate auxiliar structures ---*/

                    delete[] nChildren_MPI;
                    delete[] Parent_Remote;
                    delete[] Children_Remote;
                    delete[] Parent_Local;
                    delete[] Children_Local;

                    /*--- Deallocate receive buffer ---*/

                    delete[] Buffer_Receive_Children;
                    delete[] Buffer_Receive_Parent;
                }
            }

            /*--- Update the number of points after the MPI agglomeration ---*/
            nPoint = Index_CoarseCV;

            /*--- Console output with the summary of the agglomeration ---*/
            Local_nPointCoarse = nPoint;
            Local_nPointFine = fine_grid->GetnPoint();

#ifdef HAVE_MPI
            MPI_Allreduce(&Local_nPointCoarse, &Global_nPointCoarse, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
            MPI_Allreduce(&Local_nPointFine, &Global_nPointFine, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
#else
            Global_nPointCoarse = Local_nPointCoarse;
            Global_nPointFine = Local_nPointFine;
#endif

            double Coeff = 1.0, CFL = 0.0, factor = 1.5;

            if (iMesh != TBOX::MESH_0)
            {
                if (nDim == 2) Coeff = pow(double(Global_nPointFine) / double(Global_nPointCoarse), 1. / 2.);
                if (nDim == 3) Coeff = pow(double(Global_nPointFine) / double(Global_nPointCoarse), 1. / 3.);
                CFL = factor*config->GetCFL(iMesh - 1) / Coeff;
                config->SetCFL(iMesh, CFL);
            }

            double ratio = double(Global_nPointFine) / double(Global_nPointCoarse);

            if (((nDim == 2) && (ratio < 2.5)) ||
                ((nDim == 3) && (ratio < 2.5)))
            {
                config->SetMGLevels(iMesh - 1);
            }
            else {
                if (rank == TBOX::MASTER_NODE)
                {
                    if (iMesh == 1) std::cout << "MG level: " << iMesh - 1 << " -> CVs: " << Global_nPointFine << ". Agglomeration rate 1/1.00. CFL " << config->GetCFL(iMesh - 1) << "." << std::endl;
                    std::cout << "MG level: " << iMesh << " -> CVs: " << Global_nPointCoarse << ". Agglomeration rate 1/" << ratio << ". CFL " << CFL << "." << std::endl;
                }
            }

            delete[] copy_marker;
        }

        GEOM_GeometryMultigrid::~GEOM_GeometryMultigrid(void)
        {
        }

        bool GEOM_GeometryMultigrid::SetBoundAgglomeration(unsigned long CVPoint, short marker_seed, GEOM_Geometry *fine_grid, TBOX::TBOX_Config *config)
        {
            bool agglomerate_CV = false;
            unsigned short counter, jMarker;

            unsigned short nMarker_Max = config->GetnMarker_Max();

            unsigned short *copy_marker = new unsigned short[nMarker_Max];

            /*--- Basic condition, the element has not being previously agglomerated, it belongs to the domain,
            and has passed some basic geometrical check ---*/

            if ((fine_grid->node[CVPoint]->GetAgglomerate() == false) &&
                (fine_grid->node[CVPoint]->GetDomain()) &&
                (GeometricalCheck(CVPoint, fine_grid, config)))
            {
                /*--- If the element belong to the boundary, we must be careful ---*/
                if (fine_grid->node[CVPoint]->GetBoundary())
                {

                    /*--- Identify the markers of the vertex that we want to agglomerate ---*/
                    counter = 0;
                    for (jMarker = 0; jMarker < fine_grid->GetnMarker(); jMarker++)
                        if (fine_grid->node[CVPoint]->GetVertex(jMarker) != -1)
                        {
                            copy_marker[counter] = jMarker;
                            counter++;
                        }

                    /*--- The basic condition is that the aglomerated vertex must have the same physical marker,
                    but eventually a send-receive condition ---*/

                    /*--- Only one marker in the vertex that is going to be aglomerated ---*/
                    if (counter == 1)
                    {
                        /*--- We agglomerate if there is only a marker and is the same marker as the seed marker ---*/

                        if (copy_marker[0] == marker_seed)
                            agglomerate_CV = true;

                        /*--- If there is only a marker, but the marker is the SEND_RECEIVE ---*/

                        if (config->GetMarker_All_KindBC(copy_marker[0]) == TBOX::SEND_RECEIVE)
                            agglomerate_CV = true;

                    }

                    /*--- If there are two markers in the vertex that is going to be aglomerated ---*/

                    if (counter == 2) {

                        /*--- First we verify that the seed is a physical boundary ---*/

                        if (config->GetMarker_All_KindBC(marker_seed) != TBOX::SEND_RECEIVE)
                        {

                            /*--- Then we check that one of the marker is equal to the seed marker, and the other is send/receive ---*/

                            if (((copy_marker[0] == marker_seed) && (config->GetMarker_All_KindBC(copy_marker[1]) == TBOX::SEND_RECEIVE)) ||
                                ((config->GetMarker_All_KindBC(copy_marker[0]) == TBOX::SEND_RECEIVE) && (copy_marker[1] == marker_seed)))
                                agglomerate_CV = true;
                        }
                    }
                }
                /*--- If the element belong to the domain, it is allways aglomerated ---*/
                else
                {
                    agglomerate_CV = true;
                }
            }

            return agglomerate_CV;

            delete[] copy_marker;
        }

        bool GEOM_GeometryMultigrid::GeometricalCheck(unsigned long iPoint, GEOM_Geometry *fine_grid, TBOX::TBOX_Config *config)
        {
            double max_dimension = 1.2;

            /*--- Evaluate the total size of the element ---*/
            bool Volume = true;
            double ratio = pow(fine_grid->node[iPoint]->GetVolume(), 1.0 / double(nDim))*max_dimension;
            double limit = pow(config->GetDomainVolume(), 1.0 / double(nDim));
            if (ratio > limit) Volume = false;

            /*--- Evaluate the stretching of the element ---*/
            bool Stretching = true;

            /*	unsigned short iNode, iDim;
            unsigned long jPoint;
            double *Coord_i = fine_grid->node[iPoint]->GetCoord();
            double max_dist = 0.0 ; double min_dist = 1E20;
            for (iNode = 0; iNode <	fine_grid->node[iPoint]->GetnPoint(); iNode ++) {
            jPoint = fine_grid->node[iPoint]->GetPoint(iNode);
            double *Coord_j = fine_grid->node[jPoint]->GetCoord();
            double distance = 0.0;
            for (iDim = 0; iDim < nDim; iDim++)
            distance += (Coord_j[iDim]-Coord_i[iDim])*(Coord_j[iDim]-Coord_i[iDim]);
            distance = sqrt(distance);
            max_dist = max(distance, max_dist);
            min_dist = min(distance, min_dist);
            }
            if ( max_dist/min_dist > 100.0 ) Stretching = false;*/

            return (Stretching && Volume);
        }

        void GEOM_GeometryMultigrid::SetSuitableNeighbors(std::vector<unsigned long> *Suitable_Indirect_Neighbors, unsigned long iPoint, unsigned long Index_CoarseCV, GEOM_Geometry *fine_grid)
        {

            unsigned long jPoint, kPoint, lPoint;
            unsigned short iNode, jNode, iNeighbor, jNeighbor, kNode;
            bool SecondNeighborSeed, ThirdNeighborSeed;
            std::vector<unsigned long>::iterator it;

            /*--- Create a list with the first neighbors, including the seed ---*/
            std::vector<unsigned long> First_Neighbor_Points;
            First_Neighbor_Points.push_back(iPoint);
            for (iNode = 0; iNode < fine_grid->node[iPoint]->GetnPoint(); iNode++)
            {
                jPoint = fine_grid->node[iPoint]->GetPoint(iNode);
                First_Neighbor_Points.push_back(jPoint);
            }

            /*--- Create a list with the second neighbors, without first, and seed neighbors ---*/
            std::vector<unsigned long> Second_Neighbor_Points, Second_Origin_Points, Suitable_Second_Neighbors;

            for (iNode = 0; iNode < fine_grid->node[iPoint]->GetnPoint(); iNode++)
            {
                jPoint = fine_grid->node[iPoint]->GetPoint(iNode);

                for (jNode = 0; jNode < fine_grid->node[jPoint]->GetnPoint(); jNode++)
                {
                    kPoint = fine_grid->node[jPoint]->GetPoint(jNode);

                    /*--- Check that the second neighbor do not belong to the first neighbor or the seed ---*/

                    SecondNeighborSeed = true;
                    for (iNeighbor = 0; iNeighbor < First_Neighbor_Points.size(); iNeighbor++)
                        if (kPoint == First_Neighbor_Points[iNeighbor])
                        {
                            SecondNeighborSeed = false; break;
                        }

                    if (SecondNeighborSeed)
                    {
                        Second_Neighbor_Points.push_back(kPoint);
                        Second_Origin_Points.push_back(jPoint);
                    }
                }
            }

            /*---  Identify those second neighbors that are repeated (candidate to be added) ---*/
            for (iNeighbor = 0; iNeighbor < Second_Neighbor_Points.size(); iNeighbor++)
                for (jNeighbor = 0; jNeighbor < Second_Neighbor_Points.size(); jNeighbor++)
                    /*--- Repeated second neighbor with different origin ---*/
                    if ((Second_Neighbor_Points[iNeighbor] == Second_Neighbor_Points[jNeighbor]) &&
                        (Second_Origin_Points[iNeighbor] != Second_Origin_Points[jNeighbor]) &&
                        (iNeighbor < jNeighbor))
                    {
                        Suitable_Indirect_Neighbors->push_back(Second_Neighbor_Points[iNeighbor]);

                        /*--- Create alist with the suitable second neighbor, that we will use
                        to compute the third neighbors --*/
                        Suitable_Second_Neighbors.push_back(Second_Neighbor_Points[iNeighbor]);
                    }


            /*--- Remove repeated from the suitable second neighbors ---*/
            sort(Suitable_Second_Neighbors.begin(), Suitable_Second_Neighbors.end());
            it = unique(Suitable_Second_Neighbors.begin(), Suitable_Second_Neighbors.end());
            Suitable_Second_Neighbors.resize(it - Suitable_Second_Neighbors.begin());

            /*--- Remove repeated from first neighbors ---*/
            sort(First_Neighbor_Points.begin(), First_Neighbor_Points.end());
            it = unique(First_Neighbor_Points.begin(), First_Neighbor_Points.end());
            First_Neighbor_Points.resize(it - First_Neighbor_Points.begin());

            /*--- Create a list with the third neighbors, without first, second, and seed neighbors ---*/
            std::vector<unsigned long> Third_Neighbor_Points, Third_Origin_Points;

            for (jNode = 0; jNode < Suitable_Second_Neighbors.size(); jNode++)
            {
                kPoint = Suitable_Second_Neighbors[jNode];

                for (kNode = 0; kNode < fine_grid->node[kPoint]->GetnPoint(); kNode++)
                {
                    lPoint = fine_grid->node[kPoint]->GetPoint(kNode);

                    /*--- Check that the third neighbor do not belong to the first neighbors or the seed ---*/
                    ThirdNeighborSeed = true;

                    for (iNeighbor = 0; iNeighbor < First_Neighbor_Points.size(); iNeighbor++)
                        if (lPoint == First_Neighbor_Points[iNeighbor])
                        {
                            ThirdNeighborSeed = false;
                            break;
                        }

                    /*--- Check that the third neighbor do not belong to the second neighbors ---*/
                    for (iNeighbor = 0; iNeighbor < Suitable_Second_Neighbors.size(); iNeighbor++)
                        if (lPoint == Suitable_Second_Neighbors[iNeighbor])
                        {
                            ThirdNeighborSeed = false;
                            break;
                        }

                    if (ThirdNeighborSeed)
                    {
                        Third_Neighbor_Points.push_back(lPoint);
                        Third_Origin_Points.push_back(kPoint);
                    }

                }
            }

            /*---  Identify those third neighbors that are repeated (candidate to be added) ---*/
            for (iNeighbor = 0; iNeighbor < Third_Neighbor_Points.size(); iNeighbor++)
                for (jNeighbor = 0; jNeighbor < Third_Neighbor_Points.size(); jNeighbor++)

                    /*--- Repeated second neighbor with different origin ---*/
                    if ((Third_Neighbor_Points[iNeighbor] == Third_Neighbor_Points[jNeighbor]) &&
                        (Third_Origin_Points[iNeighbor] != Third_Origin_Points[jNeighbor]) &&
                        (iNeighbor < jNeighbor))
                    {
                        Suitable_Indirect_Neighbors->push_back(Third_Neighbor_Points[iNeighbor]);
                    }

            /*--- Remove repeated from Suitable Indirect Neighbors List ---*/
            sort(Suitable_Indirect_Neighbors->begin(), Suitable_Indirect_Neighbors->end());
            it = unique(Suitable_Indirect_Neighbors->begin(), Suitable_Indirect_Neighbors->end());
            Suitable_Indirect_Neighbors->resize(it - Suitable_Indirect_Neighbors->begin());
        }

        void GEOM_GeometryMultigrid::SetPoint_Connectivity(GEOM_Geometry *fine_grid)
        {
            unsigned long iFinePoint, iFinePoint_Neighbor, iParent, iCoarsePoint;
            unsigned short iChildren, iNode;

            /*--- Set the point surrounding a point ---*/
            for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint++)
            {
                for (iChildren = 0; iChildren < node[iCoarsePoint]->GetnChildren_CV(); iChildren++)
                {
                    iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
                    for (iNode = 0; iNode < fine_grid->node[iFinePoint]->GetnPoint(); iNode++)
                    {
                        iFinePoint_Neighbor = fine_grid->node[iFinePoint]->GetPoint(iNode);
                        iParent = fine_grid->node[iFinePoint_Neighbor]->GetParent_CV();
                        if (iParent != iCoarsePoint) node[iCoarsePoint]->SetPoint(iParent);
                    }
                }
            }

            /*--- Set the number of neighbors variable, this is
            important for JST and multigrid in parallel ---*/
            for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint++)
                node[iCoarsePoint]->SetnNeighbor(node[iCoarsePoint]->GetnPoint());
        }

        void GEOM_GeometryMultigrid::SetVertex(GEOM_Geometry *fine_grid, TBOX::TBOX_Config *config)
        {
            unsigned long  iVertex, iFinePoint, iCoarsePoint;
            unsigned short iMarker, iMarker_Tag, iChildren;

            nMarker = fine_grid->GetnMarker();
            unsigned short nMarker_Max = config->GetnMarker_Max();

            /*--- If any children node belong to the boundary then the entire control
            volume will belong to the boundary ---*/
            for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint++)
                for (iChildren = 0; iChildren < node[iCoarsePoint]->GetnChildren_CV(); iChildren++)
                {
                    iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
                    if (fine_grid->node[iFinePoint]->GetBoundary())
                    {
                        node[iCoarsePoint]->SetBoundary(nMarker);
                        break;
                    }
                }

            vertex = new GRID::GRID_DGVertex**[nMarker];
            nVertex = new unsigned long[nMarker];

            Tag_to_Marker = new std::string[nMarker_Max];
            for (iMarker_Tag = 0; iMarker_Tag < nMarker_Max; iMarker_Tag++)
                Tag_to_Marker[iMarker_Tag] = fine_grid->GetMarker_Tag(iMarker_Tag);

            /*--- Compute the number of vertices to do the dimensionalization ---*/
            for (iMarker = 0; iMarker < nMarker; iMarker++) nVertex[iMarker] = 0;

            for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint++)
            {
                if (node[iCoarsePoint]->GetBoundary())
                {
                    for (iChildren = 0; iChildren < node[iCoarsePoint]->GetnChildren_CV(); iChildren++)
                    {
                        iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
                        for (iMarker = 0; iMarker < nMarker; iMarker++)
                        {
                            if ((fine_grid->node[iFinePoint]->GetVertex(iMarker) != -1) && (node[iCoarsePoint]->GetVertex(iMarker) == -1))
                            {
                                iVertex = nVertex[iMarker];
                                node[iCoarsePoint]->SetVertex(iVertex, iMarker);
                                nVertex[iMarker]++;
                            }
                        }
                    }
                }
            }

            for (iMarker = 0; iMarker < nMarker; iMarker++)
            {
                vertex[iMarker] = new GRID::GRID_DGVertex*[fine_grid->GetnVertex(iMarker) + 1];
                nVertex[iMarker] = 0;
            }

            for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint++)
                if (node[iCoarsePoint]->GetBoundary())
                    for (iMarker = 0; iMarker < nMarker; iMarker++)
                        node[iCoarsePoint]->SetVertex(-1, iMarker);

            for (iMarker = 0; iMarker < nMarker; iMarker++) nVertex[iMarker] = 0;

            for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint++)
                if (node[iCoarsePoint]->GetBoundary())
                    for (iChildren = 0; iChildren < node[iCoarsePoint]->GetnChildren_CV(); iChildren++)
                    {
                        iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
                        for (iMarker = 0; iMarker < fine_grid->GetnMarker(); iMarker++)
                        {
                            if ((fine_grid->node[iFinePoint]->GetVertex(iMarker) != -1) && (node[iCoarsePoint]->GetVertex(iMarker) == -1))
                            {
                                iVertex = nVertex[iMarker];
                                vertex[iMarker][iVertex] = new GRID::GRID_DGVertex(iCoarsePoint, nDim);
                                node[iCoarsePoint]->SetVertex(iVertex, iMarker);

                                /*--- Set the transformation to apply ---*/
                                unsigned long ChildVertex = fine_grid->node[iFinePoint]->GetVertex(iMarker);
                                unsigned short RotationKind = fine_grid->vertex[iMarker][ChildVertex]->GetRotation_Type();
                                vertex[iMarker][iVertex]->SetRotation_Type(RotationKind);
                                nVertex[iMarker]++;
                            }
                        }
                    }
        }

        void GEOM_GeometryMultigrid::MatchNearField(TBOX::TBOX_Config *config)
        {
            unsigned short iMarker;
            unsigned long iVertex, iPoint;
            int iProcessor;

#ifndef HAVE_MPI
            iProcessor = TBOX::MASTER_NODE;
#else
            MPI_Comm_rank(MPI_COMM_WORLD, &iProcessor);
#endif

            for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
            {
                if (config->GetMarker_All_KindBC(iMarker) == TBOX::NEARFIELD_BOUNDARY)
                {
                    for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++)
                    {
                        iPoint = vertex[iMarker][iVertex]->GetNode();
                        if (node[iPoint]->GetDomain())
                        {
                            vertex[iMarker][iVertex]->SetDonorPoint(iPoint, iProcessor);
                        }
                    }
                }
            }

        }

        void GEOM_GeometryMultigrid::MatchActuator_Disk(TBOX::TBOX_Config *config)
        {
            unsigned short iMarker;
            unsigned long iVertex, iPoint;
            int iProcessor;

#ifndef HAVE_MPI
            iProcessor = TBOX::MASTER_NODE;
#else
            MPI_Comm_rank(MPI_COMM_WORLD, &iProcessor);
#endif

            for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
            {
                if ((config->GetMarker_All_KindBC(iMarker) == TBOX::ACTDISK_INLET) ||
                    (config->GetMarker_All_KindBC(iMarker) == TBOX::ACTDISK_OUTLET))
                {
                    for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++)
                    {
                        iPoint = vertex[iMarker][iVertex]->GetNode();
                        if (node[iPoint]->GetDomain())
                        {
                            vertex[iMarker][iVertex]->SetDonorPoint(iPoint, iProcessor);
                        }
                    }
                }
            }

        }

        void GEOM_GeometryMultigrid::MatchInterface(TBOX::TBOX_Config *config)
        {

            unsigned short iMarker;
            unsigned long iVertex, iPoint;
            int iProcessor;

#ifndef HAVE_MPI
            iProcessor = TBOX::MASTER_NODE;
#else
            MPI_Comm_rank(MPI_COMM_WORLD, &iProcessor);
#endif

            for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
            {
                if (config->GetMarker_All_KindBC(iMarker) == TBOX::INTERFACE_BOUNDARY)
                {
                    for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++)
                    {
                        iPoint = vertex[iMarker][iVertex]->GetNode();
                        if (node[iPoint]->GetDomain())
                        {
                            vertex[iMarker][iVertex]->SetDonorPoint(iPoint, iProcessor);
                        }
                    }
                }
            }
        }


        void GEOM_GeometryMultigrid::SetControlVolume(TBOX::TBOX_Config *config, GEOM_Geometry *fine_grid, unsigned short action)
        {
            unsigned long iFinePoint, iFinePoint_Neighbor, iCoarsePoint, iEdge, iParent;
            long FineEdge, CoarseEdge;
            unsigned short iChildren, iNode, iDim;
            bool change_face_orientation;
            double *Normal, Coarse_Volume, Area, *NormalFace = NULL;
            Normal = new double[nDim];

            /*--- Compute the area of the coarse volume ---*/
            for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint++)
            {
                node[iCoarsePoint]->SetVolume(0.0);
                Coarse_Volume = 0.0;
                for (iChildren = 0; iChildren < node[iCoarsePoint]->GetnChildren_CV(); iChildren++)
                {
                    iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
                    Coarse_Volume += fine_grid->node[iFinePoint]->GetVolume();
                }
                node[iCoarsePoint]->SetVolume(Coarse_Volume);
            }

            /*--- Update or not the values of faces at the edge ---*/
            if (action != TBOX::ALLOCATE)
            {
                for (iEdge = 0; iEdge < nEdge; iEdge++)
                    edge[iEdge]->SetZeroValues();
            }

            for (iCoarsePoint = 0; iCoarsePoint < nPoint; iCoarsePoint++)
                for (iChildren = 0; iChildren < node[iCoarsePoint]->GetnChildren_CV(); iChildren++)
                {
                    iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);

                    for (iNode = 0; iNode < fine_grid->node[iFinePoint]->GetnPoint(); iNode++)
                    {
                        iFinePoint_Neighbor = fine_grid->node[iFinePoint]->GetPoint(iNode);
                        iParent = fine_grid->node[iFinePoint_Neighbor]->GetParent_CV();
                        if ((iParent != iCoarsePoint) && (iParent < iCoarsePoint))
                        {

                            FineEdge = fine_grid->FindEdge(iFinePoint, iFinePoint_Neighbor);

                            change_face_orientation = false;
                            if (iFinePoint < iFinePoint_Neighbor) change_face_orientation = true;

                            CoarseEdge = FindEdge(iParent, iCoarsePoint);

                            fine_grid->edge[FineEdge]->GetNormal(Normal);

                            if (change_face_orientation)
                            {
                                for (iDim = 0; iDim < nDim; iDim++) Normal[iDim] = -Normal[iDim];
                                edge[CoarseEdge]->AddNormal(Normal);
                            }
                            else
                            {
                                edge[CoarseEdge]->AddNormal(Normal);
                            }
                        }
                    }
                }
            delete[] Normal;

            /*--- Check if there is a normal with null area ---*/

            for (iEdge = 0; iEdge < nEdge; iEdge++)
            {
                NormalFace = edge[iEdge]->GetNormal();
                Area = 0.0; for (iDim = 0; iDim < nDim; iDim++) Area += NormalFace[iDim] * NormalFace[iDim];
                Area = sqrt(Area);
                if (Area == 0.0) for (iDim = 0; iDim < nDim; iDim++) NormalFace[iDim] = TBOX::EPS*TBOX::EPS;
            }

        }

        void GEOM_GeometryMultigrid::SetBoundControlVolume(TBOX::TBOX_Config *config, GEOM_Geometry *fine_grid, unsigned short action)
        {
            unsigned long iCoarsePoint, iFinePoint, FineVertex, iVertex;
            unsigned short iMarker, iChildren, iDim;
            double *Normal, Area, *NormalFace = NULL;

            Normal = new double[nDim];

            if (action != TBOX::ALLOCATE)
            {
                for (iMarker = 0; iMarker < nMarker; iMarker++)
                    for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++)
                        vertex[iMarker][iVertex]->SetZeroValues();
            }

            for (iMarker = 0; iMarker < nMarker; iMarker++)
                for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++)
                {
                    iCoarsePoint = vertex[iMarker][iVertex]->GetNode();
                    for (iChildren = 0; iChildren < node[iCoarsePoint]->GetnChildren_CV(); iChildren++)
                    {
                        iFinePoint = node[iCoarsePoint]->GetChildren_CV(iChildren);
                        if (fine_grid->node[iFinePoint]->GetVertex(iMarker) != -1)
                        {
                            FineVertex = fine_grid->node[iFinePoint]->GetVertex(iMarker);
                            fine_grid->vertex[iMarker][FineVertex]->GetNormal(Normal);
                            vertex[iMarker][iVertex]->AddNormal(Normal);
                        }
                    }
                }

            delete[] Normal;

            /*--- Check if there is a normal with null area ---*/
            for (iMarker = 0; iMarker < nMarker; iMarker++)
                for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++)
                {
                    NormalFace = vertex[iMarker][iVertex]->GetNormal();
                    Area = 0.0; for (iDim = 0; iDim < nDim; iDim++) Area += NormalFace[iDim] * NormalFace[iDim];
                    Area = sqrt(Area);
                    if (Area == 0.0) for (iDim = 0; iDim < nDim; iDim++) NormalFace[iDim] = TBOX::EPS*TBOX::EPS;
                }
        }

        void GEOM_GeometryMultigrid::SetCoord(GEOM_Geometry *geometry)
        {
            unsigned long Point_Fine, Point_Coarse;
            unsigned short iChildren, iDim;
            double Area_Parent, Area_Children;
            double *Coordinates_Fine, *Coordinates;
            Coordinates = new double[nDim];

            for (Point_Coarse = 0; Point_Coarse < GetnPoint(); Point_Coarse++)
            {
                Area_Parent = node[Point_Coarse]->GetVolume();
                for (iDim = 0; iDim < nDim; iDim++) Coordinates[iDim] = 0.0;
                for (iChildren = 0; iChildren < node[Point_Coarse]->GetnChildren_CV(); iChildren++)
                {
                    Point_Fine = node[Point_Coarse]->GetChildren_CV(iChildren);
                    Area_Children = geometry->node[Point_Fine]->GetVolume();
                    Coordinates_Fine = geometry->node[Point_Fine]->GetCoord();
                    for (iDim = 0; iDim < nDim; iDim++)
                        Coordinates[iDim] += Coordinates_Fine[iDim] * Area_Children / Area_Parent;
                }
                for (iDim = 0; iDim < nDim; iDim++)
                    node[Point_Coarse]->SetCoord(iDim, Coordinates[iDim]);
            }
            delete[] Coordinates;
        }

        void GEOM_GeometryMultigrid::SetRotationalVelocity(TBOX::TBOX_Config *config)
        {

            unsigned long iPoint_Coarse;
            double *RotVel, Distance[3] = { 0.0, 0.0, 0.0 }, *Coord, Center[3] = { 0.0, 0.0, 0.0 }, Omega[3] = { 0.0, 0.0, 0.0 }, L_Ref;

            RotVel = new double[3];

            /*--- Center of rotation & angular velocity std::vector from config. ---*/

            Center[0] = config->GetMotion_Origin_X(TBOX::ZONE_0);
            Center[1] = config->GetMotion_Origin_Y(TBOX::ZONE_0);
            Center[2] = config->GetMotion_Origin_Z(TBOX::ZONE_0);
            Omega[0] = config->GetRotation_Rate_X(TBOX::ZONE_0) / config->GetOmega_Ref();
            Omega[1] = config->GetRotation_Rate_Y(TBOX::ZONE_0) / config->GetOmega_Ref();
            Omega[2] = config->GetRotation_Rate_Z(TBOX::ZONE_0) / config->GetOmega_Ref();
            L_Ref = config->GetLength_Ref();

            /*--- Loop over all nodes and set the rotational velocity. ---*/

            for (iPoint_Coarse = 0; iPoint_Coarse < GetnPoint(); iPoint_Coarse++)
            {

                /*--- Get the coordinates of the current node ---*/

                Coord = node[iPoint_Coarse]->GetCoord();

                /*--- Calculate the non-dim. distance from the rotation center ---*/

                Distance[0] = (Coord[0] - Center[0]) / L_Ref;
                Distance[1] = (Coord[1] - Center[1]) / L_Ref;
                Distance[2] = (Coord[2] - Center[2]) / L_Ref;

                /*--- Calculate the angular velocity as omega X r ---*/

                RotVel[0] = Omega[1] * (Distance[2]) - Omega[2] * (Distance[1]);
                RotVel[1] = Omega[2] * (Distance[0]) - Omega[0] * (Distance[2]);
                RotVel[2] = Omega[0] * (Distance[1]) - Omega[1] * (Distance[0]);

                /*--- Store the grid velocity at this node ---*/

                node[iPoint_Coarse]->SetGridVel(RotVel);

            }

            delete[] RotVel;

        }

        void GEOM_GeometryMultigrid::SetGridVelocity(TBOX::TBOX_Config *config, unsigned long iter)
        {

            /*--- Local variables ---*/

            double *Coord_nP1 = NULL, *Coord_n = NULL, *Coord_nM1 = NULL;
            double TimeStep, GridVel = 0.0;
            unsigned long Point_Coarse;
            unsigned short iDim;

            /*--- Compute the velocity of each node in the volume mesh ---*/

            for (Point_Coarse = 0; Point_Coarse < GetnPoint(); Point_Coarse++)
            {

                /*--- Coordinates of the current point at n+1, n, & n-1 time levels ---*/

                Coord_nM1 = node[Point_Coarse]->GetCoord_n1();
                Coord_n = node[Point_Coarse]->GetCoord_n();
                Coord_nP1 = node[Point_Coarse]->GetCoord();

                /*--- Unsteady time step ---*/

                TimeStep = config->GetDelta_UnstTimeND();

                /*--- Compute mesh velocity with 1st or 2nd-order approximation ---*/

                for (iDim = 0; iDim < nDim; iDim++)
                {
                    if (config->GetUnsteady_Simulation() == TBOX::DT_STEPPING_1ST)
                        GridVel = (Coord_nP1[iDim] - Coord_n[iDim]) / TimeStep;
                    if (config->GetUnsteady_Simulation() == TBOX::DT_STEPPING_2ND)
                        GridVel = (3.0*Coord_nP1[iDim] - 4.0*Coord_n[iDim]
                        + 1.0*Coord_nM1[iDim]) / (2.0*TimeStep);

                    /*--- Store grid velocity for this point ---*/

                    node[Point_Coarse]->SetGridVel(iDim, GridVel);

                }
            }
        }

        void GEOM_GeometryMultigrid::SetRestricted_GridVelocity(GEOM_Geometry *fine_mesh, TBOX::TBOX_Config *config)
        {

            /*--- Local variables ---*/
            unsigned short iDim, iChild;
            unsigned long Point_Coarse, Point_Fine;
            double Area_Parent, Area_Child, Grid_Vel[3], *Grid_Vel_Fine;

            /*--- Loop over all coarse mesh points ---*/
            for (Point_Coarse = 0; Point_Coarse < GetnPoint(); Point_Coarse++)
            {
                Area_Parent = node[Point_Coarse]->GetVolume();

                /*--- Zero out the grid velocity ---*/
                for (iDim = 0; iDim < nDim; iDim++)
                    Grid_Vel[iDim] = 0.0;

                /*--- Loop over all of the children for this coarse CV and compute
                a grid velocity based on the values in the child CVs (fine mesh). ---*/
                for (iChild = 0; iChild < node[Point_Coarse]->GetnChildren_CV(); iChild++)
                {
                    Point_Fine = node[Point_Coarse]->GetChildren_CV(iChild);
                    Area_Child = fine_mesh->node[Point_Fine]->GetVolume();
                    Grid_Vel_Fine = fine_mesh->node[Point_Fine]->GetGridVel();
                    for (iDim = 0; iDim < nDim; iDim++)
                        Grid_Vel[iDim] += Grid_Vel_Fine[iDim] * Area_Child / Area_Parent;
                }

                /*--- Set the grid velocity for this coarse node. ---*/
                for (iDim = 0; iDim < nDim; iDim++)
                    node[Point_Coarse]->SetGridVel(iDim, Grid_Vel[iDim]);
            }
        }


        void GEOM_GeometryMultigrid::FindNormal_Neighbor(TBOX::TBOX_Config *config)
        {

            unsigned short iMarker, iDim;
            unsigned long iPoint, iVertex;

            for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
            {

                if (config->GetMarker_All_KindBC(iMarker) != TBOX::SEND_RECEIVE &&
                    config->GetMarker_All_KindBC(iMarker) != TBOX::INTERFACE_BOUNDARY &&
                    config->GetMarker_All_KindBC(iMarker) != TBOX::NEARFIELD_BOUNDARY)
                {

                    for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++)
                    {

                        iPoint = vertex[iMarker][iVertex]->GetNode();

                        /*--- If the node belong to the domain ---*/
                        if (node[iPoint]->GetDomain())
                        {
                            /*--- Compute closest normal neighbor ---*/
                            double cos_max, scalar_prod, norm_vect, norm_Normal, cos_alpha, diff_coord;
                            unsigned long Point_Normal = 0, jPoint;
                            unsigned short iNeigh;
                            double *Normal = vertex[iMarker][iVertex]->GetNormal();
                            cos_max = -1.0;
                            for (iNeigh = 0; iNeigh < node[iPoint]->GetnPoint(); iNeigh++)
                            {
                                jPoint = node[iPoint]->GetPoint(iNeigh);
                                scalar_prod = 0.0; norm_vect = 0.0; norm_Normal = 0.0;
                                for (iDim = 0; iDim < nDim; iDim++)
                                {
                                    diff_coord = node[jPoint]->GetCoord(iDim) - node[iPoint]->GetCoord(iDim);
                                    scalar_prod += diff_coord*Normal[iDim];
                                    norm_vect += diff_coord*diff_coord;
                                    norm_Normal += Normal[iDim] * Normal[iDim];
                                }
                                norm_vect = sqrt(norm_vect);
                                norm_Normal = sqrt(norm_Normal);
                                cos_alpha = scalar_prod / (norm_vect*norm_Normal);

                                /*--- Get maximum cosine (not minimum because normals are oriented inwards) ---*/
                                if (cos_alpha >= cos_max)
                                {
                                    Point_Normal = jPoint;
                                    cos_max = cos_alpha;
                                }
                            }
                            vertex[iMarker][iVertex]->SetNormal_Neighbor(Point_Normal);
                        }
                    }
                }
            }
        }


        void GEOM_GeometryMultigrid::SetGeometryPlanes(TBOX::TBOX_Config *config)
        {
            bool loop_on;
            unsigned short iMarker = 0;
            double auxXCoord, auxYCoord, auxZCoord, *Face_Normal = NULL, auxArea, *Xcoord = NULL, *Ycoord = NULL, *Zcoord = NULL, *FaceArea = NULL;
            unsigned long jVertex, iVertex, ixCoord, iPoint, iVertex_Wall, nVertex_Wall = 0;

            /*--- Compute the total number of points on the near-field ---*/
            nVertex_Wall = 0;
            for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
                if ((config->GetMarker_All_KindBC(iMarker) == TBOX::HEAT_FLUX) ||
                    (config->GetMarker_All_KindBC(iMarker) == TBOX::HEAT_FLUX_CATALYTIC) ||
                    (config->GetMarker_All_KindBC(iMarker) == TBOX::HEAT_FLUX_NONCATALYTIC) ||
                    (config->GetMarker_All_KindBC(iMarker) == TBOX::ISOTHERMAL) ||
                    (config->GetMarker_All_KindBC(iMarker) == TBOX::ISOTHERMAL_CATALYTIC) ||
                    (config->GetMarker_All_KindBC(iMarker) == TBOX::ISOTHERMAL_NONCATALYTIC) ||
                    (config->GetMarker_All_KindBC(iMarker) == TBOX::EULER_WALL))
                    nVertex_Wall += nVertex[iMarker];


            /*--- Create an array with all the coordinates, points, pressures, face area,
            equivalent area, and nearfield weight ---*/
            Xcoord = new double[nVertex_Wall];
            Ycoord = new double[nVertex_Wall];
            if (nDim == 3)	Zcoord = new double[nVertex_Wall];
            FaceArea = new double[nVertex_Wall];

            /*--- Copy the boundary information to an array ---*/
            iVertex_Wall = 0;
            for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
                if ((config->GetMarker_All_KindBC(iMarker) == TBOX::HEAT_FLUX) ||
                    (config->GetMarker_All_KindBC(iMarker) == TBOX::HEAT_FLUX_CATALYTIC) ||
                    (config->GetMarker_All_KindBC(iMarker) == TBOX::HEAT_FLUX_NONCATALYTIC) ||
                    (config->GetMarker_All_KindBC(iMarker) == TBOX::ISOTHERMAL) ||
                    (config->GetMarker_All_KindBC(iMarker) == TBOX::ISOTHERMAL_CATALYTIC) ||
                    (config->GetMarker_All_KindBC(iMarker) == TBOX::ISOTHERMAL_NONCATALYTIC) ||
                    (config->GetMarker_All_KindBC(iMarker) == TBOX::EULER_WALL))
                    for (iVertex = 0; iVertex < nVertex[iMarker]; iVertex++)
                    {
                        iPoint = vertex[iMarker][iVertex]->GetNode();
                        Xcoord[iVertex_Wall] = node[iPoint]->GetCoord(0);
                        Ycoord[iVertex_Wall] = node[iPoint]->GetCoord(1);
                        if (nDim == 3) Zcoord[iVertex_Wall] = node[iPoint]->GetCoord(2);
                        Face_Normal = vertex[iMarker][iVertex]->GetNormal();
                        FaceArea[iVertex_Wall] = fabs(Face_Normal[nDim - 1]);
                        iVertex_Wall++;
                    }


            //std::vector<double> XCoordList;
            std::vector<double>::iterator IterXCoordList;

            for (iVertex = 0; iVertex < nVertex_Wall; iVertex++)
                XCoordList.push_back(Xcoord[iVertex]);

            sort(XCoordList.begin(), XCoordList.end());
            IterXCoordList = unique(XCoordList.begin(), XCoordList.end());
            XCoordList.resize(IterXCoordList - XCoordList.begin());

            /*--- Create std::vectors and distribute the values among the different PhiAngle queues ---*/
            Xcoord_plane.resize(XCoordList.size());
            Ycoord_plane.resize(XCoordList.size());
            if (nDim == 3) Zcoord_plane.resize(XCoordList.size());
            FaceArea_plane.resize(XCoordList.size());
            Plane_points.resize(XCoordList.size());


            double dist_ratio;
            unsigned long iCoord;

            /*--- Distribute the values among the different PhiAngles ---*/
            for (iPoint = 0; iPoint < nPoint; iPoint++)
            {
                if (node[iPoint]->GetDomain())
                {
                    loop_on = true;
                    for (ixCoord = 0; ixCoord < XCoordList.size() - 1 && loop_on; ixCoord++)
                    {
                        dist_ratio = (node[iPoint]->GetCoord(0) - XCoordList[ixCoord]) / (XCoordList[ixCoord + 1] - XCoordList[ixCoord]);
                        if (dist_ratio >= 0 && dist_ratio <= 1.0)
                        {
                            if (dist_ratio <= 0.5) iCoord = ixCoord;
                            else iCoord = ixCoord + 1;
                            Xcoord_plane[iCoord].push_back(node[iPoint]->GetCoord(0));
                            Ycoord_plane[iCoord].push_back(node[iPoint]->GetCoord(1));
                            if (nDim == 3) Zcoord_plane[iCoord].push_back(node[iPoint]->GetCoord(2));
                            FaceArea_plane[iCoord].push_back(node[iPoint]->GetVolume());   ///// CHECK AREA CALCULATION
                            Plane_points[iCoord].push_back(iPoint);
                            loop_on = false;
                        }
                    }
                }
            }

            unsigned long auxPoint;
            /*--- Order the arrays in ascending values of y ---*/
            for (ixCoord = 0; ixCoord < XCoordList.size(); ixCoord++)
                for (iVertex = 0; iVertex < Xcoord_plane[ixCoord].size(); iVertex++)
                    for (jVertex = 0; jVertex < Xcoord_plane[ixCoord].size() - 1 - iVertex; jVertex++)
                        if (Ycoord_plane[ixCoord][jVertex] > Ycoord_plane[ixCoord][jVertex + 1])
                        {
                            auxXCoord = Xcoord_plane[ixCoord][jVertex]; Xcoord_plane[ixCoord][jVertex] = Xcoord_plane[ixCoord][jVertex + 1]; Xcoord_plane[ixCoord][jVertex + 1] = auxXCoord;
                            auxYCoord = Ycoord_plane[ixCoord][jVertex]; Ycoord_plane[ixCoord][jVertex] = Ycoord_plane[ixCoord][jVertex + 1]; Ycoord_plane[ixCoord][jVertex + 1] = auxYCoord;
                            auxPoint = Plane_points[ixCoord][jVertex]; Plane_points[ixCoord][jVertex] = Plane_points[ixCoord][jVertex + 1]; Plane_points[ixCoord][jVertex + 1] = auxPoint;
                            if (nDim == 3)
                            {
                                auxZCoord = Zcoord_plane[ixCoord][jVertex]; Zcoord_plane[ixCoord][jVertex] = Zcoord_plane[ixCoord][jVertex + 1]; Zcoord_plane[ixCoord][jVertex + 1] = auxZCoord;
                            }
                            auxArea = FaceArea_plane[ixCoord][jVertex]; FaceArea_plane[ixCoord][jVertex] = FaceArea_plane[ixCoord][jVertex + 1]; FaceArea_plane[ixCoord][jVertex + 1] = auxArea;
                        }

            /*--- Delete structures ---*/
            delete[] Xcoord; delete[] Ycoord;
            if (nDim == 3) delete[] Zcoord;
            delete[] FaceArea;
        }
    }
}
/*
 *================================================================================
 *
 *    Copyright (c) 2016 Vortex Co.,Ltd.
 *    Unpublished - All rights reserved
 *
 *================================================================================
 *    File description:
 *    Class for pyramid grid definition
 *
 *================================================================================
 *    Date            Name                    Description of Change
 *    23-Sep-2016     Jiamin Xu               Creation
 *================================================================================
 */

#ifndef ARIES_GRIDPYRAMID_HPP
#define ARIES_GRIDPYRAMID_HPP

#include "Grid.hpp"

namespace ARIES
{
    class GridPyramid: public Grid
    {
    public:
        GridPyramid(unsigned long val_point_0, unsigned long val_point_1,
                    unsigned long val_point_2, unsigned long val_point_3,
                    unsigned long val_point_4);

        ~GridPyramid(void);

        unsigned long GetNode(unsigned short val_node) { return d_node[val_node]; };
        void SetNode(unsigned short val_node, unsigned long val_point) { d_node[val_node] = val_point; };
  
        unsigned short GetFace(unsigned short val_face, unsigned short val_index) { return d_face[val_face][val_index]; };
        unsigned short GetNeighborNode(unsigned short val_node, unsigned short val_index) { return d_neighborNode[val_node][val_index];};

        unsigned short GetNumNeighborNode(unsigned short val_node) { return d_numNeighborNode[val_node]; };
        unsigned short GetNumNodeFace(unsigned short val_face) { return d_numNodeFace[val_face]; };

        unsigned short GetNumNode() { return d_numNode; };
        unsigned short GetNumFace() { return d_numFace; };
        unsigned short GetMaxNodeFace() { return d_maxNodeFace; };
 
        unsigned short GetVTKType() { return d_VTKType; };
        unsigned short GetNumNeighborElement() { return d_numNeighborElement; };

        void ChangeOrientation();

    private:

        static unsigned short d_face[5][4];			/*!< \brief Matrix to store the local nodes of all the faces. */
        static unsigned short d_neighborNode[5][4];	/*!< \brief Neighbor to a nodes in the element. */
        static unsigned short d_numNodeFace[5];		/*!< \brief Number of nodes of each face of the element. */
        static unsigned short d_numNeighborNode[5];	/*!< \brief Number of Neighbor to a nodes in the element. */
        static unsigned short d_numFace;				/*!< \brief Number of faces of the element. */
        static unsigned short d_numNode;				/*!< \brief Number of nodes of the element. */
        static unsigned short d_VTKType;				/*!< \brief Type of element using VTK nomenclature. */
        static unsigned short d_maxNodeFace;			/*!< \brief Maximum number of nodes for a face. */
        static unsigned short d_numNeighborElement;	/*!< \brief Number of neighbor elements. */
    };
    
}

#endif

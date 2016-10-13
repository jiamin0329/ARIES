/*
 *================================================================================
 *
 *    Copyright (c) 2016 Vortex Co.,Ltd.
 *    Unpublished - All rights reserved
 *
 *================================================================================
 *    File description:
 *    Class for fem element definition, base class of fem elements.
 *
 *================================================================================
 *    Date            Name                    Description of Change
 *    03-Oct-2016     Jiamin Xu               Creation
 *================================================================================
 */

#ifndef ARIES_ELEM_HPP
#define ARIES_ELEM_HPP


namespace ARIES
{
    class Elem
    {

    public:
        /*!
         * \brief Constructor of the class.
         */
        CElement(void);

        /*!
         * \overload
         * \param[in] val_nDim - Number of dimensions of the problem.
         * \param[in] config - Definition of the particular problem.
         */
        CElement(unsigned short val_nDim, CConfig *config);

        /*!
         * \brief Destructor of the class.
         */
        virtual ~CElement(void);

        /*!
         * \brief Retrieve the number of nodes of the element.
         * \param[out] nNodes - Number of nodes of the element.
         */
        unsigned short GetnNodes(void);

        /*!
         * \brief Retrieve the number of nodes of the element.
         * \param[out] nGaussPoints - Number of Gaussian Points of the element.
         */
        unsigned short GetnGaussPoints(void);

        /*!
         * \brief Retrieve the number of nodes of the element.
         * \param[out] nGaussPointsP - Number of Gaussian Points for pressure underintegration.
         */
        unsigned short GetnGaussPointsP(void);

        /*!
         * \brief Set the value of the coordinate of the nodes in the reference configuration.
         * \param[in] val_CoordRef - Value of the coordinate.
         * \param[in] iNode - Number of node.
         * \param[in] iDim - Dimension
         */
        void SetRef_Coord(su2double val_CoordRef, unsigned short iNode, unsigned short iDim);

        /*!
         * \brief Set the value of the coordinate of the nodes in the current configuration.
         * \param[in] val_CoordRef - Value of the coordinate.
         * \param[in] iNode - Number of node.
         * \param[in] iDim - Dimension
         */
        void SetCurr_Coord(su2double val_CoordCurr, unsigned short iNode, unsigned short iDim);

        /*!
         * \brief Set the value of the pressure in the element for incompressible materials.
         * \param[in] val_ElPressure - Value of the pressure.
         */
        void SetElement_Pressure(su2double val_ElPressure);

        /*!
         * \brief Set the value of the coordinate of the nodes in the reference configuration.
         * \param[in] val_CoordRef - Value of the coordinate.
         * \param[in] iNode - Number of node.
         * \param[in] iDim - Dimension
         * \param[out] Coordinate
         */
        su2double GetRef_Coord(unsigned short iNode, unsigned short iDim);

        /*!
         * \brief Get the value of the coordinate of the nodes in the current configuration.
         * \param[in] val_CoordRef - Value of the coordinate.
         * \param[in] iNode - Number of node.
         * \param[in] iDim - Dimension
         * \param[out] Coordinate
         */
        su2double GetCurr_Coord(unsigned short iNode, unsigned short iDim);

        /*!
         * \brief Get the weight of the corresponding Gaussian Point.
         * \param[in] iGauss - index of the Gaussian point.
         * \param[out] Weight.
         */
        su2double GetWeight(unsigned short iGauss);

        /*!
         * \brief Get the weight of the corresponding Gaussian Point for pressure subintegration.
         * \param[in] iGaussP - index of the Gaussian point.
         * \param[out] Weight.
         */
        su2double GetWeight_P(unsigned short iGaussP);

        /*!
         * \brief Get the jacobian respect to the reference configuration for the Gaussian Point iGauss.
         * \param[in] iGauss - index of the Gaussian point.
         * \param[out] Weight.
         */
        su2double GetJ_X(unsigned short iGauss);

        /*!
         * \brief Get the jacobian respect to the current configuration for the Gaussian Point iGauss.
         * \param[in] iGauss - index of the Gaussian point.
         * \param[out] Weight.
         */
        su2double GetJ_x(unsigned short iGauss);

        /*!
         * \brief Get the jacobian respect to the reference configuration for the Gaussian Point iGauss and the pressure term.
         * \param[in] iGauss - index of the Gaussian point.
         * \param[out] Weight.
         */
        su2double GetJ_X_P(unsigned short iGauss);

        /*!
         * \brief Get the jacobian respect to the current configuration for the Gaussian Point iGauss and the pressure term.
         * \param[in] iGauss - index of the Gaussian point.
         * \param[out] Weight.
         */
        su2double GetJ_x_P(unsigned short iGauss);

        /*!
         * \brief Retrieve the value of the pressure in the element for incompressible materials.
         * \param[out] Value of the pressure.
         */
        su2double GetElement_Pressure(void);

        /*!
         * \brief Add the value of the diagonal term for the mass matrix.
         * \param[in] nodeA - index of Node a.
         * \param[in] nodeB - index of Node b.
         * \param[in] val_Ks_ab - value of the term that will constitute the diagonal of the stress contribution.
         */
        void Add_Mab(su2double val_Mab, unsigned short nodeA, unsigned short nodeB);

        /*!
         * \brief Add the value of a submatrix K relating nodes a and b, for the constitutive term.
         * \param[in] nodeA - index of Node a.
         * \param[in] nodeB - index of Node b.
         * \param[in] val_Kab - value of the matrix K.
         */
        void Add_Kab(su2double **val_Kab, unsigned short nodeA, unsigned short nodeB);

        /*!
         * \brief Add the value of a submatrix K relating nodes a and b, for the constitutive term (symmetric terms need transpose)
         * \param[in] nodeA - index of Node a.
         * \param[in] nodeB - index of Node b.
         * \param[in] val_Kab - value of the matrix K.
         */
        void Add_Kab_T(su2double **val_Kab, unsigned short nodeA, unsigned short nodeB);


        /*!
         * \brief Add the value of the diagonal term for the stress contribution to the stiffness of the system.
         * \param[in] nodeA - index of Node a.
         * \param[in] nodeB - index of Node b.
         * \param[in] val_Ks_ab - value of the term that will constitute the diagonal of the stress contribution.
         */
        void Add_Ks_ab(su2double val_Ks_ab, unsigned short nodeA, unsigned short nodeB);

        /*!
         * \brief Add the value of the nodal stress term for the computation of the residual.
         * \param[in] nodeA - index of Node a.
         * \param[in] val_Kt_a - value of the term that will constitute the diagonal of the stress contribution.
         */
        void Add_Kt_a(su2double *val_Kt_a, unsigned short nodeA);

        /*!
         * \brief Add the value of the dead load for the computation of the residual.
         * \param[in] nodeA - index of Node a.
         * \param[in] val_FDL_a - value of the term that will constitute the diagonal of the stress contribution.
         */
        void Add_FDL_a(su2double *val_FDL_a, unsigned short nodeA);

        /*!
         * \brief Set the value of a submatrix K relating nodes a and b, for the pressure term (this term is subintegrated).
         * \param[in] nodeA - index of Node a.
         * \param[in] nodeB - index of Node b.
         * \param[in] val_Kab - value of the matrix K.
         */
        void Set_Kk_ab(su2double **val_Kk_ab, unsigned short nodeA, unsigned short nodeB);

        /*!
         * \brief Restarts the values in the element.
         */
        void clearElement(void);

        /*!
         * \brief Restarts the values of stress in the element.
         */
        void clearStress(void);

        /*!
         * \brief Return the value of the diagonal term for the mass matrix, relating nodes a and b.
         * \param[in] nodeA - index of Node a.
         * \param[in] nodeB - index of Node b.
         * \param[out] val_Mab - value of the diagonal term of Mab.
         */
        su2double Get_Mab(unsigned short nodeA, unsigned short nodeB);

        /*!
         * \brief Return the value of the submatrix K relating nodes a and b.
         * \param[in] nodeA - index of Node a.
         * \param[in] nodeB - index of Node b.
         * \param[out] val_Kab - value of the matrix K.
         */
        su2double *Get_Kab(unsigned short nodeA, unsigned short nodeB);

        /*!
         * \brief Return the value of the diagonal term for the stress contribution, relating nodes a and b.
         * \param[in] nodeA - index of Node a.
         * \param[in] nodeB - index of Node b.
         * \param[out] val_Kab - value of the matrix K.
         */
        su2double Get_Ks_ab(unsigned short nodeA, unsigned short nodeB);

        /*!
         * \brief Return the value of a submatrix K relating nodes a and b, for the pressure term (this term is subintegrated).
         * \param[in] nodeA - index of Node a.
         * \param[in] nodeB - index of Node b.
         * \param[in] val_Kab - value of the matrix K.
         */
        su2double *Get_Kk_ab(unsigned short nodeA, unsigned short nodeB);

        /*!
         * \brief Return the value of the nodal stress component of the residual for node a.
         * \param[in] nodeA - index of Node a.
         * \param[out] val_Kt_a - value of the stress term.
         */
        su2double *Get_Kt_a(unsigned short nodeA);

        /*!
         * \brief Return the value of the dead load component of the residual for node a.
         * \param[in] nodeA - index of Node a.
         * \param[out] val_Kt_a - value of the stress term.
         */
        su2double *Get_FDL_a(unsigned short nodeA);

        /*!
         * \brief Retrieve the value of the shape functions.
         * \param[in] iNode - Index of the node.
         * \param[in] iNode - Index of the Gaussian Point.
         * \param[out] GradNi_X - Gradient of the shape function related to node iNode and evaluated at Gaussian Point iGauss
         */
        su2double GetNi(unsigned short iNode, unsigned short iGauss);

        /*!
         * \brief Retrieve the value of the gradient of the shape functions respect to the reference configuration.
         * \param[in] iNode - Index of the node.
         * \param[in] iNode - Index of the Gaussian Point.
         * \param[out] GradNi_X - Gradient of the shape function related to node iNode and evaluated at Gaussian Point iGauss
         */
        su2double GetGradNi_X(unsigned short iNode, unsigned short iGauss, unsigned short iDim);

        /*!
         * \brief Retrieve the value of the gradient of the shape functions respect to the reference configuration.
         * \param[in] iNode - Index of the node.
         * \param[in] iNode - Index of the Gaussian Point.
         * \param[out] GradNi_X - Gradient of the shape function related to node iNode and evaluated at Gaussian Point iGauss
         */
        su2double GetGradNi_x(unsigned short iNode, unsigned short iGauss, unsigned short iDim);

        /*!
         * \brief Retrieve the value of the gradient of the shape functions respect to the reference configuration.
         * \param[in] iNode - Index of the node.
         * \param[in] iNode - Index of the Gaussian Point.
         * \param[out] GradNi_x - Gradient of the shape function related to node iNode and evaluated at Gaussian Point iGauss
         */
        su2double GetGradNi_x_P(unsigned short iNode, unsigned short iGaussP, unsigned short iDim);

        /*!
         * \brief Retrieve the value of the gradient of the shape functions respect to the reference configuration.
         * \param[in] iNode - Index of the node.
         * \param[in] iGauss - Index of the Gaussian Point.
         * \param[out] val_Ni_Ext - Value of the shape function at the nodes for extrapolation purposes
         */
        su2double GetNi_Extrap(unsigned short iNode, unsigned short iGauss);

        /*!
         * \brief Add a value to the nodal stress for an element.
         * \param[in] iNode - Index of the node.
         * \param[in] iGauss - Index of the variable.
         * \param[in] val_Stress - Value of the stress added.
         */
        void Add_NodalStress(su2double val_Stress, unsigned short iNode, unsigned short iVar);

        /*!
         * \brief Retrieve the value of the nodal stress for an element.
         * \param[in] iNode - Index of the node.
         * \param[in] iGauss - Index of the variable.
         * \param[in] val_Stress - Value of the stress added.
         */
        su2double Get_NodalStress(unsigned short iNode, unsigned short iVar);

        /*!
         * \brief Set the value of the gradient of the shape functions respect to the reference configuration.
         * \param[in] val_solution - Solution of the problem.
         * \param[out] J_X - Jacobian of the element evaluated at the current Gauss Point respect to the reference configuration
         */
        virtual void ComputeGrad_Linear(void);

        /*!
         * \brief Set the value of the gradient of the shape functions respect to the current configuration.
         * \param[in] val_solution - Solution of the problem.
         * \param[out] J_x - Jacobian of the element evaluated at the current Gauss Point respect to the current configuration
         */
        virtual void ComputeGrad_NonLinear(void);

        /*!
         * \brief Virtual member
         */
        virtual void ComputeGrad_Pressure(void);
    protected:
        unsigned short nGaussPoints;		/*!< \brief Number of gaussian points. */
        unsigned short nGaussPointsP;		/*!< \brief Number of gaussian points for the pressure term. */
        unsigned short nNodes;				/*!< \brief Number of gaussian points. */
        static unsigned short nDim;		/*!< \brief Number of dimension of the problem. */
        su2double **CurrentCoord,				/*!< \brief Coordinates in the current frame. */
            **RefCoord;							/*!< \brief Coordinates in the reference frame. */
        su2double **GaussCoord,				/*!< \brief Parent coordinates of the Gaussian Points. */
            *GaussWeight;						/*!< \brief Weight of the Gaussian Points for the integration. */
        su2double	**GaussCoordP,				/*!< \brief Parent coordinates of the Gaussian Points for the pressure subintegration.. */
            *GaussWeightP;						/*!< \brief Weight of the Gaussian Points for the pressure subintegration. */
        su2double **NodalExtrap;				/*!< \brief Coordinates of the nodal points for Gaussian extrapolation */
        su2double **NodalStress;				/*!< \brief Stress at the nodes */
        CGaussVariable **GaussPoint;		/*!< \brief Structure for the Gaussian Points. */
        CGaussVariable **GaussPointP;		/*!< \brief Structure for the Gaussian Points for the pressure subintegration. */
        su2double **Mab;						/*!< \brief Structure for the nodal components of the mass matrix. */
        su2double ***Kab;						/*!< \brief Structure for the constitutive component of the tangent matrix. */
        su2double **Ks_ab;						/*!< \brief Structure for the stress component of the tangent matrix. */
        su2double ***Kk_ab;					/*!< \brief Structure for the pressure component of the tangent matrix. */
        su2double **Kt_a;						/*!< \brief Structure for the nodal stress term for the residual computation. */
        su2double **FDL_a;						/*!< \brief Structure for the dead loads for the residual computation. */
        su2double el_Pressure;					/*!< \brief Pressure in the element */


    };
}

#endif

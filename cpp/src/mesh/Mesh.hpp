#ifndef MESH_HPP
#define MESH_HPP

#include "Navigation.hpp"

#include <Eigen/Dense>
#include <geogram/mesh/mesh.h>

namespace poly_fem
{
	enum class ElementType {
		RegularInteriorCube,        // Regular square/hex inside a 3^n patch
		SimpleSingularInteriorCube, // Square/hex incident to exactly 1 singular vertex (in 2D) or edge (in 3D)
		MultiSingularInteriorCube,  // Square/Hex incident to more than 1 singular vertices (should not happen in 2D)
		RegularBoundaryCube,        // Boundary square/hex, where all boundary vertices are incident to at most 2 squares/hexes
		SingularBoundaryCube,       // Boundary square/hex that is not regular
		InteriorPolytope,           // Interior polytope
		BoundaryPolytope,           // Boundary polytope
	};

	class Mesh
	{
	public:
		virtual ~Mesh() { }

		virtual void refine(const int n_refiniment) = 0;

		virtual inline bool is_volume() const = 0;

		virtual inline int n_elements() const = 0;
		virtual inline int n_pts() const = 0;

		virtual inline int n_element_vertices(const int element_index) const = 0;
		virtual inline int vertex_global_index(const int element_index, const int local_index) const = 0;

		virtual double compute_mesh_size() const = 0;

		virtual void triangulate_faces(Eigen::MatrixXi &tris, Eigen::MatrixXd &pts, std::vector<int> &ranges) const = 0;

		virtual void set_boundary_tags(std::vector<int> &tags) const = 0;

		virtual void point(const int global_index, Eigen::MatrixXd &pt) const = 0;

		virtual bool load(const std::string &path) = 0;
		virtual bool save(const std::string &path) const = 0;

		virtual void get_edges(Eigen::MatrixXd &p0, Eigen::MatrixXd &p1) const = 0;

		virtual void compute_element_tag(std::vector<ElementType> &ele_tag) const = 0;

		virtual void compute_barycenter(Eigen::MatrixXd &barycenters) const = 0;
	};
}

#endif //MESH_HPP

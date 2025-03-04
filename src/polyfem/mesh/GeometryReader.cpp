#include "GeometryReader.hpp"

#include <polyfem/mesh/Mesh.hpp>
#include <polyfem/mesh/MeshUtils.hpp>
#include <polyfem/io/MshReader.hpp>
#include <polyfem/utils/StringUtils.hpp>

#include <polyfem/utils/JSONUtils.hpp>
#include <polyfem/utils/Selection.hpp>
#include <polyfem/utils/Logger.hpp>

#include <Eigen/Core>

#include <igl/edges.h>
#include <igl/boundary_facets.h>

namespace polyfem::mesh
{
	using namespace polyfem::utils;

	std::unique_ptr<Mesh> read_fem_mesh(
		const json &j_mesh,
		const std::string &root_path,
		const bool non_conforming)
	{
		if (!is_param_valid(j_mesh, "mesh"))
			log_and_throw_error(fmt::format("Mesh {} is mising a \"mesh\" field!", j_mesh));

		if (j_mesh["extract"].get<std::string>() != "volume")
			log_and_throw_error("Only volumetric elements are implemented for FEM meshes!");

		std::unique_ptr<Mesh> mesh = Mesh::create(resolve_path(j_mesh["mesh"], root_path), non_conforming);

		// --------------------------------------------------------------------

		// NOTE: Normaliziation is done before transformations are applied and/or any selection operators
		if (j_mesh["advanced"]["normalize_mesh"])
			mesh->normalize();

		// --------------------------------------------------------------------

		Selection::BBox bbox;
		mesh->bounding_box(bbox[0], bbox[1]);

		{
			MatrixNd A;
			VectorNd b;
			construct_affine_transformation(
				j_mesh["transformation"],
				(bbox[1] - bbox[0]).cwiseAbs().transpose(),
				A, b);
			mesh->apply_affine_transformation(A, b);
		}

		mesh->bounding_box(bbox[0], bbox[1]);

		// --------------------------------------------------------------------

		const int n_refs = j_mesh["n_refs"];
		const double refinement_location = j_mesh["advanced"]["refinement_location"];
		// TODO: renable this
		// if (n_refs <= 0 && args["poly_bases"] == "MFSHarmonic" && mesh->has_poly())
		// {
		// 	if (args["force_no_ref_for_harmonic"])
		// 		logger().warn("Using harmonic bases without refinement");
		// 	else
		// 		n_refs = 1;
		// }
		if (n_refs > 0)
		{
			// Check if the stored volume selection is uniform.
			assert(mesh->n_elements() > 0);
			const int uniform_value = mesh->get_body_id(0);
			for (int i = 1; i < mesh->n_elements(); ++i)
				if (mesh->get_body_id(i) != uniform_value)
					log_and_throw_error(fmt::format("Unable to apply stored nonuniform volume_selection because n_refs={} > 0!", n_refs));

			logger().info("Performing global h-refinement with {} refinements", n_refs);
			mesh->refine(n_refs, refinement_location);
			mesh->set_body_ids(std::vector<int>(mesh->n_elements(), uniform_value));
		}

		// --------------------------------------------------------------------

		if (j_mesh["advanced"]["min_component"].get<int>() != -1)
			log_and_throw_error("Option \"min_component\" in geometry not implement yet!");
		// TODO:
		// if (args["min_component"] > 0) {
		// 	Eigen::SparseMatrix<int> adj;
		// 	igl::facet_adjacency_matrix(boundary_triangles, adj);
		// 	Eigen::MatrixXi C, counts;
		// 	igl::connected_components(adj, C, counts);
		// 	std::vector<int> valid;
		// 	const int min_count = args["min_component"];
		// 	for (int i = 0; i < counts.size(); ++i) {
		// 		if (counts(i) >= min_count) {
		// 			valid.push_back(i);
		// 		}
		// 	}
		// 	tris.clear();
		// 	for (int i = 0; i < C.size(); ++i) {
		// 		for (int v : valid) {
		// 			if (v == C(i)) {
		// 				tris.emplace_back(boundary_triangles(i, 0), boundary_triangles(i, 1), boundary_triangles(i, 2));
		// 				break;
		// 			}
		// 		}
		// 	}
		// 	boundary_triangles.resize(tris.size(), 3);
		// 	for (int i = 0; i < tris.size(); ++i) {
		// 		boundary_triangles.row(i) << std::get<0>(tris[i]), std::get<1>(tris[i]), std::get<2>(tris[i]);
		// 	}
		// }

		// --------------------------------------------------------------------

		if (j_mesh["advanced"]["force_linear_geometry"].get<bool>())
			log_and_throw_error("Option \"force_linear_geometry\" in geometry not implement yet!");
		// TODO:
		// if (!iso_parametric()) {
		// 	if (args["force_linear_geometry"] || mesh->orders().size() <= 0) {
		// 		geom_disc_orders.resizeLike(disc_orders);
		// 		geom_disc_orders.setConstant(1);
		// 	} else {
		// 		geom_disc_orders = mesh->orders();
		// 	}
		// }

		// --------------------------------------------------------------------

		if (!j_mesh["point_selection"].is_null())
			logger().warn("Geometry point seleections are not implemented nor used!");

		if (!j_mesh["curve_selection"].is_null())
			logger().warn("Geometry point seleections are not implemented nor used!");

		// --------------------------------------------------------------------

		std::vector<std::shared_ptr<Selection>> surface_selections =
			Selection::build_selections(j_mesh["surface_selection"], bbox, root_path);

		mesh->compute_boundary_ids([&](const size_t p_id, const std::vector<int> &vs, const RowVectorNd &p, bool is_boundary) {
			if (!is_boundary)
				return -1;

			for (const auto &selection : surface_selections)
			{
				if (selection->inside(p_id, vs, p))
					return selection->id(p_id, vs, p);
			}
			return std::numeric_limits<int>::max(); // default for no selected boundary
		});

		// --------------------------------------------------------------------

		// If the selection is of the form {"id_offset": ...}
		const json volume_selection = j_mesh["volume_selection"];
		if (volume_selection.is_object()
			&& volume_selection.size() == 1
			&& volume_selection.contains("id_offset"))
		{
			const int id_offset = volume_selection["id_offset"].get<int>();
			if (id_offset != 0)
			{
				const int n_body_ids = mesh->n_elements();
				std::vector<int> body_ids(n_body_ids);
				for (int i = 0; i < n_body_ids; ++i)
					body_ids[i] = mesh->get_body_id(i) + id_offset;
				mesh->set_body_ids(body_ids);
			}
		}
		else
		{
			// Specified volume selection has priority over mesh's stored ids
			std::vector<std::shared_ptr<Selection>> volume_selections =
				Selection::build_selections(volume_selection, bbox, root_path);

			// Append the mesh's stored ids to the volume selection as a lowest priority selection
			if (mesh->has_body_ids())
				volume_selections.push_back(std::make_shared<SpecifiedSelection>(mesh->get_body_ids()));

			mesh->compute_body_ids([&](const size_t cell_id, const RowVectorNd &p) -> int {
				for (const auto &selection : volume_selections)
				{
					// TODO: add vs to compute_body_ids
					if (selection->inside(cell_id, {}, p))
						return selection->id(cell_id, {}, p);
				}
				return 0;
			});
		}

		// --------------------------------------------------------------------

		return mesh;
	}

	// ========================================================================

	std::unique_ptr<Mesh> read_fem_geometry(
		const json &geometry,
		const std::string &root_path,
		const std::vector<std::string> &_names,
		const std::vector<Eigen::MatrixXd> &_vertices,
		const std::vector<Eigen::MatrixXi> &_cells,
		const bool non_conforming)
	{
		// TODO: fix me for hdf5
		// {
		// 	int index = -1;
		// 	for (int i = 0; i < names.size(); ++i)
		// 	{
		// 		if (names[i] == args["meshes"])
		// 		{
		// 			index = i;
		// 			break;
		// 		}
		// 	}
		// 	assert(index >= 0);
		// 	if (vertices[index].cols() == 2)
		// 		mesh = std::make_unique<polyfem::CMesh2D>();
		// 	else
		// 		mesh = std::make_unique<polyfem::Mesh3D>();
		// 	mesh->build_from_matrices(vertices[index], cells[index]);
		// }
		assert(_names.empty());
		assert(_vertices.empty());
		assert(_cells.empty());

		// --------------------------------------------------------------------

		if (geometry.empty())
			log_and_throw_error("Provided geometry is empty!");

		std::vector<json> geometries;
		// Note you can add more types here, just add them to geometries
		if (geometry.is_object())
			geometries.push_back(geometry);
		else if (geometry.is_array())
			geometries = geometry.get<std::vector<json>>();
		else
			log_and_throw_error("Invalid JSON geometry type!");

		// --------------------------------------------------------------------

		std::unique_ptr<Mesh> mesh = nullptr;

		for (const json &geometry : geometries)
		{
			if (!geometry["enabled"].get<bool>() || geometry["is_obstacle"].get<bool>())
				continue;

			if (geometry["type"] != "mesh")
				log_and_throw_error(
					fmt::format("Invalid geometry type \"{}\" for FEM mesh!", geometry["type"]));

			if (mesh == nullptr)
				mesh = read_fem_mesh(geometry, root_path, non_conforming);
			else
				mesh->append(read_fem_mesh(geometry, root_path, non_conforming));
		}

		// --------------------------------------------------------------------

		return mesh;
	}

	// ========================================================================

	void read_obstacle_mesh(
		const json &j_mesh,
		const std::string &root_path,
		const int dim,
		Eigen::MatrixXd &vertices,
		Eigen::VectorXi &codim_vertices,
		Eigen::MatrixXi &codim_edges,
		Eigen::MatrixXi &faces)
	{
		if (!is_param_valid(j_mesh, "mesh"))
			log_and_throw_error(fmt::format("Mesh obstacle {} is mising a \"mesh\" field!", j_mesh));

		const std::string mesh_path = resolve_path(j_mesh["mesh"], root_path);

		bool read_success = read_surface_mesh(
			mesh_path, vertices, codim_vertices, codim_edges, faces);

		if (!read_success)
			// error already logged in read_surface_mesh()
			throw std::runtime_error(fmt::format("Unable to read mesh: {}", mesh_path));

		const int prev_dim = vertices.cols();
		vertices.conservativeResize(vertices.rows(), dim);
		if (prev_dim < dim)
			vertices.rightCols(dim - prev_dim).setZero();

		// --------------------------------------------------------------------

		{
			const VectorNd mesh_dimensions = (vertices.colwise().maxCoeff() - vertices.colwise().minCoeff()).cwiseAbs();
			MatrixNd A;
			VectorNd b;
			construct_affine_transformation(j_mesh["transformation"], mesh_dimensions, A, b);
			vertices = vertices * A.transpose();
			vertices.rowwise() += b.transpose();
		}

		std::string extract = j_mesh["extract"];
		// Default: "volume" clashes with defaults for non obstacle, here assume volume is suface
		if (extract == "volume")
			extract = "surface";

		if (extract == "points")
		{
			// points -> vertices (drop edges and faces)
			codim_edges.resize(0, 0);
			faces.resize(0, 0);
			codim_vertices.resize(vertices.rows());
			for (int i = 0; i < codim_vertices.size(); ++i)
				codim_vertices[i] = i;
		}
		else if (extract == "edges" && faces.size() != 0)
		{
			// edges -> edges (drop faces)
			Eigen::MatrixXi edges;
			igl::edges(faces, edges);
			faces.resize(0, 0);
			codim_edges.conservativeResize(codim_edges.rows() + edges.rows(), 2);
			codim_edges.bottomRows(edges.rows()) = edges;
		}
		else if (extract == "surface" && dim == 2 && faces.size() != 0)
		{
			// surface (2D) -> boundary edges (drop faces and interior edges)
			Eigen::MatrixXi boundary_edges;
			igl::boundary_facets(faces, boundary_edges);
			codim_edges.conservativeResize(codim_edges.rows() + boundary_edges.rows(), 2);
			codim_edges.bottomRows(boundary_edges.rows()) = boundary_edges;
			faces.resize(0, 0); // Clear faces
		}
		// surface (3D) -> boundary faces
		// No need to do anything for (extract == "surface" && dim == 3) since we used read_surface_mesh
		else if (extract == "volume")
		{
			// volume -> undefined
			log_and_throw_error("Volumetric elements not supported for collision obstacles!");
		}

		if (j_mesh["n_refs"].get<int>() != 0)
		{
			log_and_throw_error("Option \"n_refs\" in obstacles not implement yet!");
			if (j_mesh["advanced"]["refinement_location"].get<double>() != 0.5)
				log_and_throw_error("Option \"refinement_location\" in obstacles not implement yet!");
		}
	}

	// ========================================================================

	Obstacle read_obstacle_geometry(
		const json &geometry,
		const std::vector<json> &displacements,
		const std::string &root_path,
		const int dim,
		const std::vector<std::string> &_names,
		const std::vector<Eigen::MatrixXd> &_vertices,
		const std::vector<Eigen::MatrixXi> &_cells,
		const bool non_conforming)
	{
		// TODO: fix me for hdf5
		// {
		// 	int index = -1;
		// 	for (int i = 0; i < names.size(); ++i)
		// 	{
		// 		if (names[i] == args["meshes"])
		// 		{
		// 			index = i;
		// 			break;
		// 		}
		// 	}
		// 	assert(index >= 0);
		// 	if (vertices[index].cols() == 2)
		// 		mesh = std::make_unique<polyfem::CMesh2D>();
		// 	else
		// 		mesh = std::make_unique<polyfem::Mesh3D>();
		// 	mesh->build_from_matrices(vertices[index], cells[index]);
		// }
		assert(_names.empty());
		assert(_vertices.empty());
		assert(_cells.empty());

		Obstacle obstacle;

		if (geometry.empty())
			return obstacle;

		std::vector<json> geometries;
		// Note you can add more types here, just add them to geometries
		if (geometry.is_object())
		{
			geometries.push_back(geometry);
		}
		else if (geometry.is_array())
		{
			geometries = geometry.get<std::vector<json>>();
		}

		for (int i = 0; i < geometries.size(); i++)
		{
			json complete_geometry = geometries[i];

			if (!complete_geometry["is_obstacle"].get<bool>())
				continue;

			if (!complete_geometry["enabled"].get<bool>())
				continue;

			if (complete_geometry["type"] == "mesh")
			{
				Eigen::MatrixXd vertices;
				Eigen::VectorXi codim_vertices;
				Eigen::MatrixXi codim_edges;
				Eigen::MatrixXi faces;
				read_obstacle_mesh(
					complete_geometry, root_path, dim, vertices, codim_vertices,
					codim_edges, faces);

				json displacement = "{\"value\":[0, 0, 0]}"_json;
				if (is_param_valid(complete_geometry, "surface_selection"))
				{
					if (!complete_geometry["surface_selection"].is_number())
						log_and_throw_error("Invalid surface_selection for obstacle, needs to be an integer!");

					const int id = complete_geometry["surface_selection"];
					for (const json &disp : displacements)
					{
						// TODO: Add support for array of ints
						if ((disp["id"].is_string() && disp["id"].get<std::string>() == "all")
							|| (disp["id"].is_number_integer() && disp["id"].get<int>() == id))
						{
							displacement = disp;
							break;
						}
					}
				}

				obstacle.append_mesh(
					vertices, codim_vertices, codim_edges, faces, displacement);
			}
			else if (complete_geometry["type"] == "plane")
			{
				// TODO
			}
			else
			{
				log_and_throw_error(
					fmt::format("Invalid geometry type \"{}\" for obstacle!", complete_geometry["type"]));
			}
		}

		return obstacle;
	}

	// ========================================================================

	void construct_affine_transformation(
		const json &transform,
		const VectorNd &mesh_dimensions,
		MatrixNd &A,
		VectorNd &b)
	{
		const int dim = mesh_dimensions.size();

		// -----
		// Scale
		// -----

		RowVectorNd scale;
		if (transform["dimensions"].is_array()) // default is nullptr
		{
			VectorNd modified_dimensions =
				(mesh_dimensions.array() == 0).select(1, mesh_dimensions);

			scale = transform["dimensions"];
			const int scale_size = scale.size();
			scale.conservativeResize(dim);
			if (scale_size < dim)
				scale.tail(dim - scale_size).setZero();

			scale.array() /= modified_dimensions.array();
		}
		else if (transform["scale"].is_number())
		{
			scale.setConstant(dim, transform["scale"].get<double>());
		}
		else
		{
			assert(transform["scale"].is_array());
			scale = transform["scale"];
			const int scale_size = scale.size();
			scale.conservativeResize(dim);
			if (scale_size < dim)
				scale.tail(dim - scale_size).setZero();

			if (scale_size == 0)
				scale.setOnes();
		}

		A = scale.asDiagonal();

		// ------
		// Rotate
		// ------

		// Rotate around the models origin NOT the bodies center of mass.
		// We could expose this choice as a "rotate_around" field.
		MatrixNd R = MatrixNd::Identity(dim, dim);
		if (!transform["rotation"].is_null())
		{
			if (dim == 2)
			{
				if (transform["rotation"].is_number())
					R = Eigen::Rotation2Dd(deg2rad(transform["rotation"].get<double>()))
							.toRotationMatrix();
				else if (!transform["rotation"].is_array() || !transform["rotation"].empty())
					log_and_throw_error("Invalid 2D rotation; 2D rotations can only be a angle in degrees.");
			}
			else if (dim == 3)
			{
				R = to_rotation_matrix(transform["rotation"], transform["rotation_mode"]);
			}
		}

		A = R * A; // Scale first, then rotate

		// ---------
		// Translate
		// ---------

		b = transform["translation"];
		const int translation_size = b.size();
		b.conservativeResize(dim);
		if (translation_size < dim)
			b.tail(dim - translation_size).setZero();
	}

} // namespace polyfem::mesh
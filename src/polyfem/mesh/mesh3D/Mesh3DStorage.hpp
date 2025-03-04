#pragma once

#include <vector>
#include <Eigen/Dense>

namespace polyfem
{
	namespace mesh
	{
		struct Vertex
		{
			int id;
			std::vector<double> v;
			std::vector<uint32_t> neighbor_vs;
			std::vector<uint32_t> neighbor_es;
			std::vector<uint32_t> neighbor_fs;
			std::vector<uint32_t> neighbor_hs;

			bool boundary;
			bool boundary_hex;
		};
		struct Edge
		{
			int id;
			std::vector<uint32_t> vs;
			std::vector<uint32_t> neighbor_fs;
			std::vector<uint32_t> neighbor_hs;

			bool boundary;
			bool boundary_hex;
		};
		struct Face
		{
			int id;
			std::vector<uint32_t> vs;
			std::vector<uint32_t> es;
			std::vector<uint32_t> neighbor_hs;
			bool boundary;
			bool boundary_hex;
		};

		struct Element
		{
			int id;
			std::vector<uint32_t> vs;
			std::vector<uint32_t> es;
			std::vector<uint32_t> fs;
			std::vector<bool> fs_flag;
			bool hex = false;
			std::vector<double> v_in_Kernel;
		};

		enum MeshType
		{
			Tri = 0,
			Qua,
			HSur,
			Tet,
			Hyb,
			Hex
		};

		class Mesh3DStorage
		{
		public:
			MeshType type;
			Eigen::MatrixXd points;
			std::vector<Vertex> vertices;
			std::vector<Edge> edges;
			std::vector<Face> faces;
			std::vector<Element> elements;

			Eigen::MatrixXi EV;              // EV(2, ne)
			Eigen::MatrixXi FV, FE, FH, FHi; // FV (3, nf), FE(3, nf), FH (2, nf), FHi(2, nf)
			Eigen::MatrixXi HV, HF;          // HV(4, nh), HE(6, nh), HF(4, nh)

			void append(const Mesh3DStorage &other)
			{
				if (other.type != type)
					type = MeshType::Hyb;

				const int n_v = points.cols();
				const int n_e = edges.size();
				const int n_f = faces.size();
				const int n_c = elements.size();
				assert(n_v == vertices.size());

				assert(points.rows() == other.points.rows());
				points.conservativeResize(points.rows(), n_v + other.points.cols());
				points.rightCols(other.points.cols()) = other.points;

				for (const auto &v : other.vertices)
				{
					auto tmp = v;
					tmp.id += n_v;
					for (auto &e : tmp.neighbor_vs)
						e += n_v;

					for (auto &e : tmp.neighbor_es)
						e += n_e;

					for (auto &e : tmp.neighbor_fs)
						e += n_f;

					for (auto &e : tmp.neighbor_hs)
						e += n_c;

					vertices.push_back(tmp);
				}
				assert(points.cols() == vertices.size());
				assert(vertices.size() == n_v + other.vertices.size());

				for (const auto &e : other.edges)
				{
					auto tmp = e;
					tmp.id += n_e;
					for (auto &e : tmp.vs)
						e += n_v;

					for (auto &e : tmp.neighbor_fs)
						e += n_f;

					for (auto &e : tmp.neighbor_hs)
						e += n_c;

					edges.push_back(tmp);
				}
				assert(edges.size() == n_e + other.edges.size());

				for (const auto &f : other.faces)
				{
					auto tmp = f;
					tmp.id += n_f;
					for (auto &e : tmp.vs)
						e += n_v;

					for (auto &e : tmp.es)
						e += n_e;

					for (auto &e : tmp.neighbor_hs)
						e += n_c;

					faces.push_back(tmp);
				}
				assert(faces.size() == n_f + other.faces.size());

				for (const auto &c : other.elements)
				{
					auto tmp = c;
					tmp.id += n_c;
					for (auto &e : tmp.vs)
						e += n_v;

					for (auto &e : tmp.es)
						e += n_e;

					for (auto &e : tmp.fs)
						e += n_f;

					elements.push_back(tmp);
				}
				assert(elements.size() == n_c + other.elements.size());

				EV.conservativeResize(EV.rows(), other.EV.cols() + EV.cols());
				EV.rightCols(other.EV.cols()) = other.EV.array() + n_v;

				//////////////////

				FV.conservativeResize(FV.rows(), other.FV.cols() + FV.cols());
				FV.rightCols(other.FV.cols()) = other.FV.array() + n_v;

				FE.conservativeResize(FE.rows(), other.FE.cols() + FE.cols());
				FE.rightCols(other.FE.cols()) = other.FE.array() + n_e;

				FH.conservativeResize(FH.rows(), other.FH.cols() + FH.cols());
				FH.rightCols(other.FH.cols()) = other.FH.array() + n_c;

				FHi.conservativeResize(FHi.rows(), other.FHi.cols() + FHi.cols());
				FHi.rightCols(other.FHi.cols()) = other.FHi.array();

				/////////////////

				HV.conservativeResize(HV.rows(), other.HV.cols() + HV.cols());
				HV.rightCols(other.HV.cols()) = other.HV.array() + n_v;

				HF.conservativeResize(HF.rows(), other.HF.cols() + HF.cols());
				HF.rightCols(other.HF.cols()) = other.HF.array() + n_f;
			}
		};

		struct Mesh_Quality
		{
			std::string Name;
			double min_Jacobian;
			double ave_Jacobian;
			double deviation_Jacobian;
			Eigen::VectorXd V_Js;
			Eigen::VectorXd H_Js;
			Eigen::VectorXd Num_Js;
			int32_t V_num, H_num;
		};
	} // namespace mesh
} // namespace polyfem

#pragma once

#include <polyfem/assembler/ElementAssemblyValues.hpp>
#include <polyfem/assembler/AssemblerUtils.hpp>
#include <polyfem/mesh/Obstacle.hpp>

#include <polyfem/assembler/Problem.hpp>
#include <polyfem/mesh/LocalBoundary.hpp>
#include <polyfem/utils/ElasticityUtils.hpp>
#include <polyfem/utils/Types.hpp>

#include <functional>
#include <vector>

namespace polyfem
{
	namespace assembler
	{
		// computes the rhs of a problem by \int \phi rho rhs
		class RhsAssembler
		{
		public:
			// initialization with assembler factory mesh
			// size of the problem, bases
			// formulation, problem
			// and solver used internally
			RhsAssembler(const AssemblerUtils &assembler, const mesh::Mesh &mesh, const mesh::Obstacle &obstacle, const std::vector<Eigen::MatrixXd> &input_dirichlet,
						 const int n_basis, const int size,
						 const std::vector<basis::ElementBases> &bases, const std::vector<basis::ElementBases> &gbases, const AssemblyValsCache &ass_vals_cache,
						 const std::string &formulation, const Problem &problem,
						 const std::string bc_method,
						 const std::string &solver, const std::string &preconditioner, const json &solver_params);

			// computes the rhs of a problem by \int \phi rho rhs
			void assemble(const Density &density, Eigen::MatrixXd &rhs, const double t = 1) const;

			// computes the inital soltion for time dependent, calls time_bc
			void initial_solution(Eigen::MatrixXd &sol) const;
			// computes the inital velocity for time dependent, calls time_bc
			void initial_velocity(Eigen::MatrixXd &sol) const;
			// computes the inital acceleration for time dependent, calls time_bc
			void initial_acceleration(Eigen::MatrixXd &sol) const;

			// sets boundary conditions to rhs, the boundary conditions are projected (Dirichlet) integrated (Neumann) at resolution
			// local boundary stores the mapping from elemment to nodes for Dirichlet nodes
			// local local_neumann_boundary stores the mapping from elemment to nodes for Neumann nodes
			// calls set_bc
			void set_bc(const std::vector<mesh::LocalBoundary> &local_boundary, const std::vector<int> &bounday_nodes, const int resolution, const std::vector<mesh::LocalBoundary> &local_neumann_boundary, Eigen::MatrixXd &rhs, const Eigen::MatrixXd &displacement = Eigen::MatrixXd(), const double t = 1) const;

			// compute body energy
			double compute_energy(const Eigen::MatrixXd &displacement, const std::vector<mesh::LocalBoundary> &local_neumann_boundary, const Density &density, const int resolution, const double t) const;
			// compute body energy gradient, hessian is zero, rhs is a linear function
			void compute_energy_grad(const std::vector<mesh::LocalBoundary> &local_boundary, const std::vector<int> &bounday_nodes, const Density &density, const int resolution, const std::vector<mesh::LocalBoundary> &local_neumann_boundary, const Eigen::MatrixXd &final_rhs, const double t, Eigen::MatrixXd &rhs) const;

			// return the formulation
			inline const std::string &formulation() const { return formulation_; }

		private:
			// leastsquares fit bc
			void lsq_bc(const std::function<void(const Eigen::MatrixXi &, const Eigen::MatrixXd &, const Eigen::MatrixXd &, Eigen::MatrixXd &)> &df,
						const std::vector<mesh::LocalBoundary> &local_boundary, const std::vector<int> &bounday_nodes, const int resolution, Eigen::MatrixXd &rhs) const;

			// integrate bc
			void integrate_bc(const std::function<void(const Eigen::MatrixXi &, const Eigen::MatrixXd &, const Eigen::MatrixXd &, Eigen::MatrixXd &)> &df,
							  const std::vector<mesh::LocalBoundary> &local_boundary, const std::vector<int> &bounday_nodes, const int resolution, Eigen::MatrixXd &rhs) const;

			// sample bc at nodes
			void sample_bc(const std::function<void(const Eigen::MatrixXi &, const Eigen::MatrixXd &, const Eigen::MatrixXd &, Eigen::MatrixXd &)> &df,
						   const std::vector<mesh::LocalBoundary> &local_boundary, const std::vector<int> &bounday_nodes, Eigen::MatrixXd &rhs) const;

			// set boundary condition
			// the 2 lambdas are callback to dirichlet df and neumann nf
			// diriclet boundary condition are projected on the FEM bases, it inverts a linear system
			void set_bc(
				const std::function<void(const Eigen::MatrixXi &, const Eigen::MatrixXd &, const Eigen::MatrixXd &, Eigen::MatrixXd &)> &df,
				const std::function<void(const Eigen::MatrixXi &, const Eigen::MatrixXd &, const Eigen::MatrixXd &, const Eigen::MatrixXd &, Eigen::MatrixXd &)> &nf,
				const std::vector<mesh::LocalBoundary> &local_boundary, const std::vector<int> &bounday_nodes, const int resolution, const std::vector<mesh::LocalBoundary> &local_neumann_boundary, const Eigen::MatrixXd &displacement, Eigen::MatrixXd &rhs) const;

			// sets the time (initial) boundary condition
			// the lambda depeneds if soltuion, velocity, or acceleration
			// they are projected on the FEM bases, it inverts a linear system
			void time_bc(const std::function<void(const mesh::Mesh &, const Eigen::MatrixXi &, const Eigen::MatrixXd &, Eigen::MatrixXd &)> &fun, Eigen::MatrixXd &sol) const;

			const AssemblerUtils &assembler_;
			const mesh::Mesh &mesh_;
			const mesh::Obstacle &obstacle_;
			const int n_basis_;
			const int size_;
			const std::vector<basis::ElementBases> &bases_;
			const std::vector<basis::ElementBases> &gbases_;
			const AssemblyValsCache &ass_vals_cache_;
			const std::string formulation_;
			const Problem &problem_;
			const std::string bc_method_;
			const std::string solver_, preconditioner_;
			const json solver_params_;
			const std::vector<Eigen::MatrixXd> &input_dirichlet_;
		};
	} // namespace assembler
} // namespace polyfem

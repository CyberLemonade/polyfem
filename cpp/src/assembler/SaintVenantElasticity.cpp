#include "SaintVenantElasticity.hpp"

#include "Basis.hpp"
#include "ElementAssemblyValues.hpp"

#include <unsupported/Eigen/AutoDiff>

namespace poly_fem
{

	namespace
	{
		typedef Eigen::Matrix<double, Eigen::Dynamic, 1, 0> Derivative_type;
		typedef Eigen::AutoDiffScalar<Derivative_type> Scalar_type;
		typedef Eigen::Matrix<Scalar_type, Eigen::Dynamic, 1> Matrix_t;

		double von_mises_stress_for_stress_tensor(const Eigen::MatrixXd &stress)
		{
			double von_mises_stress =  0.5 * ( stress(0, 0) - stress(1, 1) ) * ( stress(0, 0) - stress(1, 1) ) + 3.0  *  stress(0, 1) * stress(0, 1);

			if(stress.rows() == 3)
			{
				von_mises_stress += 0.5 * (stress(2, 2) - stress(1, 1)) * (stress(2, 2) - stress(1, 1)) + 3.0  * stress(2, 1) * stress(2, 1);
				von_mises_stress += 0.5 * (stress(2, 2) - stress(0, 0)) * (stress(2, 2) - stress(0, 0)) + 3.0  * stress(2, 0) * stress(2, 0);
			}

			von_mises_stress = sqrt( fabs(von_mises_stress) );

			return von_mises_stress;
		}

		template<int dim>
		Eigen::Matrix<double, dim, dim> strain(const Eigen::MatrixXd &grad, const Eigen::MatrixXd &jac_it, int k, int coo)
		{
			Eigen::Matrix<double, dim, dim> jac;
			jac.setZero();
			jac.row(coo) = grad.row(k);
			jac = jac*jac_it;

			// return (jac.transpose()*jac + jac + jac.transpose())*0.5;
			return (jac + jac.transpose())*0.5;
		}
	}



	SaintVenantElasticity::SaintVenantElasticity()
	{
		set_size(size_);
	}

	void SaintVenantElasticity::set_size(const int size)
	{
		if(size == 2)
			stifness_tensor_.resize(6, 1);
		else
			stifness_tensor_.resize(21, 1);

		size_ = size;
	}

	template <typename T, unsigned long N>
	T SaintVenantElasticity::stress(const std::array<T, N> &strain, const int j) const
	{
		T res = stifness_tensor(j, 0)*strain[0];

		for(unsigned long k = 1; k < N; ++k)
			res += stifness_tensor(j, k)*strain[k];

		return res;
	}

	void SaintVenantElasticity::set_stiffness_tensor(int i, int j, const double val)
	{
		if(j < i)
		{
			int tmp=i;
			i = j;
			j = tmp;
		}
		assert(j>=i);
		const int n = size_ == 2 ? 3 : 6;
		assert(i < n);
		assert(j < n);
		assert(i >= 0);
		assert(j >= 0);
		const int index = n * i + j - i * (i + 1) / 2;
		assert(index < stifness_tensor_.size());

		stifness_tensor_(index) = val;
	}

	double SaintVenantElasticity::stifness_tensor(int i, int j) const
	{
		if(j < i)
		{
			int tmp=i;
			i = j;
			j = tmp;
		}

		assert(j>=i);
		const int n = size_ == 2 ? 3 : 6;
		assert(i < n);
		assert(j < n);
		assert(i >= 0);
		assert(j >= 0);
		const int index = n * i + j - i * (i + 1) / 2;
		assert(index < stifness_tensor_.size());

		return stifness_tensor_(index);
	}

	void SaintVenantElasticity::set_lambda_mu(const double lambda, const double mu)
	{
		if(size_ == 2)
		{
			set_stiffness_tensor(0, 0, 2*mu+lambda);
			set_stiffness_tensor(0, 1, lambda);
			set_stiffness_tensor(0, 2, 0);

			set_stiffness_tensor(1, 1, 2*mu+lambda);
			set_stiffness_tensor(1, 2, 0);

			set_stiffness_tensor(2, 2, mu);
		}
		else
		{
			set_stiffness_tensor(0, 0, 2*mu+lambda);
			set_stiffness_tensor(0, 1, lambda);
			set_stiffness_tensor(0, 2, lambda);
			set_stiffness_tensor(0, 3, 0);
			set_stiffness_tensor(0, 4, 0);
			set_stiffness_tensor(0, 5, 0);

			set_stiffness_tensor(1, 1, 2*mu+lambda);
			set_stiffness_tensor(1, 2, lambda);
			set_stiffness_tensor(1, 3, 0);
			set_stiffness_tensor(1, 4, 0);
			set_stiffness_tensor(1, 5, 0);

			set_stiffness_tensor(2, 2, 2*mu+lambda);
			set_stiffness_tensor(2, 3, 0);
			set_stiffness_tensor(2, 4, 0);
			set_stiffness_tensor(2, 5, 0);

			set_stiffness_tensor(3, 3, mu);
			set_stiffness_tensor(3, 4, 0);
			set_stiffness_tensor(3, 5, 0);

			set_stiffness_tensor(4, 4, mu);
			set_stiffness_tensor(4, 5, 0);

			set_stiffness_tensor(5, 5, mu);

		}
	}

	Eigen::Matrix<double, Eigen::Dynamic, 1, 0, 3, 1>
	SaintVenantElasticity::assemble(const ElementAssemblyValues &vals, const int j, const Eigen::MatrixXd &displacement, const Eigen::VectorXd &da) const
	{
		Eigen::Matrix<double, Eigen::Dynamic, 1> local_disp(vals.basis_values.size() * size(), 1);
		local_disp.setZero();
		for(size_t i = 0; i < vals.basis_values.size(); ++i){
			const auto &bs = vals.basis_values[i];
			for(size_t ii = 0; ii < bs.global.size(); ++ii){
				for(int d = 0; d < size(); ++d){
					local_disp(i*size() + d) += bs.global[ii].val * displacement(bs.global[ii].index*size() + d);
				}
			}
		}

		return assemble_aux(vals, j, da, local_disp);
	}

	Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, 0, Eigen::Dynamic, 3>
	SaintVenantElasticity::assemble_grad(const ElementAssemblyValues &vals, const int j, const Eigen::MatrixXd &displacement, const Eigen::VectorXd &da) const
	{

		Eigen::Matrix<double, Eigen::Dynamic, 1> local_dispv(vals.basis_values.size() * size(), 1);
		local_dispv.setZero();
		for(size_t i = 0; i < vals.basis_values.size(); ++i){
			const auto &bs = vals.basis_values[i];
			for(size_t ii = 0; ii < bs.global.size(); ++ii){
				for(int d = 0; d < size(); ++d){
					local_dispv(i*size() + d) += bs.global[ii].val * displacement(bs.global[ii].index*size() + d);
				}
			}
		}

		Matrix_t local_disp(local_dispv.rows(), 1);
		for(long i = 0; i < local_dispv.rows(); ++i){
			local_disp(i) = local_dispv(i);
		}

		//set unit vectors for the derivative directions (partial derivatives of the input vector)
		for(size_t i = 0; i < vals.basis_values.size()*size(); ++i)
		{
			local_disp(i).derivatives().resize(vals.basis_values.size()*size());
			local_disp(i).derivatives().setZero();
			local_disp(i).derivatives()(i)=1;
		}

		const auto val = assemble_aux(vals, j, da, local_disp);




		// {
		// 	auto mmat = assemble_aux(vals, j, da, local_dispv);

		// 	Eigen::MatrixXd xxx(size(),1);
		// 	for(int aa = 0; aa < size(); ++aa)
		// 		xxx(aa) = val(aa).value();
		// 	// assert((mmat - xxx).norm()< 1e-10);
		// 	if((mmat - xxx).norm() > 1e-10)
		// 		std::cout<<(mmat - xxx).norm()<<std::endl;
		// }


		Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, 0, Eigen::Dynamic, 3> res(vals.basis_values.size()*size(), size());

		for(int i = 0; i < size(); ++i)
		{
			const auto jac = val(i).derivatives();

			for(size_t jj = 0; jj < vals.basis_values.size()*size(); ++jj)
			{
				res(jj, i) = jac(jj);
			}
		}

		return res;
	}

	template <typename T>
	Eigen::Matrix<T, Eigen::Dynamic, 1, 0, 3, 1>
	SaintVenantElasticity::assemble_aux(const ElementAssemblyValues &vals, const int j,
		const Eigen::VectorXd &da, const Eigen::Matrix<T, Eigen::Dynamic, 1> &local_disp) const
	{
		const Eigen::MatrixXd &gradj = vals.basis_values[j].grad;

		// sum (C : gradi) : gradj
		Eigen::Matrix<T, Eigen::Dynamic, 1, 0, 3, 1> res(size());

		// res.setZero();

		assert(gradj.cols() == size());
		assert(size_t(gradj.rows()) ==  vals.jac_it.size());
		assert(gradj.rows() == da.size());

		bool is_res_empty = true;


		//loop over quadrature
		for(long k = 0; k < da.size(); ++k)
		{
			Eigen::Matrix<T, Eigen::Dynamic, 1, 0, 3, 1> res_k(size());

			bool is_gradd_empty = true;
			Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, 0, 3, 3> gradd(size(), size());

			for(size_t i = 0; i < vals.basis_values.size(); ++i)
			{
				const Eigen::MatrixXd &gradi = vals.basis_values[i].grad;
				assert(gradi.cols() == size());
				assert(size_t(gradi.rows()) ==  vals.jac_it.size());

				if(size() == 2)
				{
					Eigen::Matrix<double, 2, 2> tmp;

					for(int d = 0; d < size(); ++d)
					{
						tmp.setZero();
						tmp.row(d) = gradi.row(k);
						tmp = tmp*vals.jac_it[k];

						if(is_gradd_empty){
							gradd(0,0) = tmp(0,0)*local_disp(i*size() + d);
							gradd(0,1) = tmp(0,1)*local_disp(i*size() + d);
							gradd(1,0) = tmp(1,0)*local_disp(i*size() + d);
							gradd(1,1) = tmp(1,1)*local_disp(i*size() + d);
						}
						else{
							gradd(0,0) += tmp(0,0)*local_disp(i*size() + d);
							gradd(0,1) += tmp(0,1)*local_disp(i*size() + d);
							gradd(1,0) += tmp(1,0)*local_disp(i*size() + d);
							gradd(1,1) += tmp(1,1)*local_disp(i*size() + d);
						}

						is_gradd_empty = false;
					}
				}
			}

			// const auto strain_tensor = (gradd.transpose()*gradd + gradd + gradd.transpose())*0.5;
			const auto strain_tensor = (gradd + gradd.transpose())*0.5;
			std::array<T, 3> ee;
			ee[0] = strain_tensor(0,0);
			ee[1] = strain_tensor(1,1);
			ee[2] = 2*strain_tensor(0,1);

			Eigen::Matrix<T, 2, 2> sigma; sigma <<
			stress(ee, 0), stress(ee, 2),
			stress(ee, 2), stress(ee, 1);

			const auto eps_x_j = strain<2>(gradj, vals.jac_it[k], k, 0);
			const auto eps_y_j = strain<2>(gradj, vals.jac_it[k], k, 1);

			res_k(0) = (sigma * eps_x_j).trace();
			res_k(1) = (sigma * eps_y_j).trace();


			if(is_res_empty)
				res = res_k * da(k);
			else
				res += res_k * da(k);

			is_res_empty = false;

				// {
				// 	for(int d = 0; d < size(); ++d)
				// 	{
				// 		const auto eps_i = strain<T, 3>(gradi, vals.jac_it[k], local_disp(i*size() + d), k, 0);

				// 		const auto eps_x_j = strain<double, 3>(gradj, vals.jac_it[k], 1., k, 0);
				// 		const auto eps_y_j = strain<double, 3>(gradj, vals.jac_it[k], 1., k, 1);
				// 		const auto eps_z_j = strain<double, 3>(gradj, vals.jac_it[k], 1., k, 2);

				// 		std::array<T, 6> ee, e_y, e_z;
				// 		ee[0] = eps_i(0,0);
				// 		ee[1] = eps_i(1,1);
				// 		ee[2] = eps_i(2,2);
				// 		ee[3] = 2*eps_i(1,2);
				// 		ee[4] = 2*eps_i(0,2);
				// 		ee[5] = 2*eps_i(0,1);

				// 		Eigen::Matrix<T, 3, 3> sigma; sigma <<
				// 		stress(ee, 0), stress(ee, 5), stress(ee, 4),
				// 		stress(ee, 5), stress(ee, 1), stress(ee, 3),
				// 		stress(ee, 4), stress(ee, 3), stress(ee, 2);

				// 		res_k(0) += (sigma*eps_x_j).trace();
				// 		res_k(1) += (sigma*eps_y_j).trace();
				// 		res_k(2) += (sigma*eps_z_j).trace();
				// 	}
				// }
			// }

		}

		// std::cout<<"res\n"<<res<<"\n"<<std::endl;

		return res;
	}

	void SaintVenantElasticity::compute_von_mises_stresses(const ElementBases &bs, const Eigen::MatrixXd &local_pts, const Eigen::MatrixXd &displacement, Eigen::MatrixXd &stresses) const
	{
		Eigen::MatrixXd displacement_grad(size(), size());

		assert(displacement.cols() == 1);

		stresses.resize(local_pts.rows(), 1);

		ElementAssemblyValues vals;
		vals.compute(-1, size() == 3, local_pts, bs, bs);


		for(long p = 0; p < local_pts.rows(); ++p)
		{
			displacement_grad.setZero();

			for(std::size_t j = 0; j < bs.bases.size(); ++j)
			{
				const Basis &b = bs.bases[j];
				const auto &loc_val = vals.basis_values[j];

				assert(bs.bases.size() == vals.basis_values.size());
				assert(loc_val.grad.rows() == local_pts.rows());
				assert(loc_val.grad.cols() == size());

				for(int d = 0; d < size(); ++d)
				{
					for(std::size_t ii = 0; ii < b.global().size(); ++ii)
					{
						displacement_grad.row(d) += b.global()[ii].val * loc_val.grad.row(p) * displacement(b.global()[ii].index*size() + d);
					}
				}
			}

			displacement_grad = displacement_grad * vals.jac_it[p];

			// Eigen::MatrixXd strain = (displacement_grad.transpose()*displacement_grad + displacement_grad + displacement_grad.transpose())/2.;
			Eigen::MatrixXd strain = (displacement_grad + displacement_grad.transpose())/2.;
			Eigen::MatrixXd stress_tensor(size(), size());

			if(size() == 2)
			{
				std::array<double, 3> eps;
				eps[0] = strain(0,0);
				eps[1] = strain(1,1);
				eps[2] = 2*strain(0,1);


				stress_tensor <<
				stress(eps, 0), stress(eps, 2),
				stress(eps, 2), stress(eps, 1);
			}
			else
			{
				std::array<double, 6> eps;
				eps[0] = strain(0,0);
				eps[1] = strain(1,1);
				eps[2] = strain(2,2);
				eps[3] = 2*strain(1,2);
				eps[4] = 2*strain(0,2);
				eps[5] = 2*strain(0,1);

				stress_tensor <<
				stress(eps, 0), stress(eps, 5), stress(eps, 4),
				stress(eps, 5), stress(eps, 1), stress(eps, 3),
				stress(eps, 4), stress(eps, 3), stress(eps, 2);
			}

			stresses(p) = von_mises_stress_for_stress_tensor(stress_tensor);
		}
	}

	double SaintVenantElasticity::compute_energy(const ElementAssemblyValues &vals, const Eigen::MatrixXd &displacement, const Eigen::VectorXd &da) const
	{
		Eigen::MatrixXd displacement_grad(size(), size());

		assert(displacement.cols() == 1);

		const int n_pts = da.size();

		double energy = 0;

		for(long p = 0; p < n_pts; ++p)
		{
			displacement_grad.setZero();

			for(size_t i = 0; i < vals.basis_values.size(); ++i)
			{
				const auto &bs = vals.basis_values[i];
				const Eigen::MatrixXd &grad = bs.grad;
				assert(grad.cols() == size());
				assert(size_t(grad.rows()) ==  vals.jac_it.size());

				for(int d = 0; d < size(); ++d)
				{
					for(std::size_t ii = 0; ii < bs.global.size(); ++ii)
					{
						displacement_grad.row(d) += bs.global[ii].val * grad.row(p) * displacement(bs.global[ii].index*size() + d);
					}
				}
			}

			displacement_grad = displacement_grad * vals.jac_it[p];

			// Eigen::MatrixXd strain = (displacement_grad.transpose()*displacement_grad + displacement_grad + displacement_grad.transpose())/2.;
			Eigen::MatrixXd strain = (displacement_grad + displacement_grad.transpose())/2.;
			Eigen::MatrixXd stress_tensor(size(), size());

			if(size() == 2)
			{
				std::array<double, 3> eps;
				eps[0] = strain(0,0);
				eps[1] = strain(1,1);
				eps[2] = 2*strain(0,1);


				stress_tensor <<
				stress(eps, 0), stress(eps, 2),
				stress(eps, 2), stress(eps, 1);
			}
			else
			{
				std::array<double, 6> eps;
				eps[0] = strain(0,0);
				eps[1] = strain(1,1);
				eps[2] = strain(2,2);
				eps[3] = 2*strain(1,2);
				eps[4] = 2*strain(0,2);
				eps[5] = 2*strain(0,1);

				stress_tensor <<
				stress(eps, 0), stress(eps, 5), stress(eps, 4),
				stress(eps, 5), stress(eps, 1), stress(eps, 3),
				stress(eps, 4), stress(eps, 3), stress(eps, 2);
			}

			energy += (stress_tensor * strain).trace() * da(p);
		}

		return energy;
	}




	//explicit instantiation
	template double SaintVenantElasticity::stress(const std::array<double, 3> &strain, const int j) const;
	template double SaintVenantElasticity::stress(const std::array<double, 6> &strain, const int j) const;

	template Scalar_type SaintVenantElasticity::stress(const std::array<Scalar_type, 3> &strain, const int j) const;
	template Scalar_type SaintVenantElasticity::stress(const std::array<Scalar_type, 6> &strain, const int j) const;

	template Eigen::Matrix<double, Eigen::Dynamic, 1, 0, 3, 1> SaintVenantElasticity::assemble_aux(const ElementAssemblyValues &vals, const int j, const Eigen::VectorXd &da, const Eigen::Matrix<double, Eigen::Dynamic, 1> &local_disp) const;
	template Eigen::Matrix<Scalar_type, Eigen::Dynamic, 1, 0, 3, 1> SaintVenantElasticity::assemble_aux(const ElementAssemblyValues &vals, const int j, const Eigen::VectorXd &da, const Eigen::Matrix<Scalar_type, Eigen::Dynamic, 1> &local_disp) const;
}

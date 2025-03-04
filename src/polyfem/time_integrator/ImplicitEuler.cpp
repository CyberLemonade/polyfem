#include "ImplicitEuler.hpp"

namespace polyfem::time_integrator
{
	void ImplicitEuler::update_quantities(const Eigen::VectorXd &x)
	{
		const Eigen::VectorXd v = compute_velocity(x);
		a_prev() = compute_acceleration(v);
		v_prev() = v;
		x_prev() = x;
	}

	Eigen::VectorXd ImplicitEuler::x_tilde() const
	{
		return x_prev() + dt() * v_prev();
	}

	Eigen::VectorXd ImplicitEuler::compute_velocity(const Eigen::VectorXd &x) const
	{
		return (x - x_prev()) / dt();
	}

	Eigen::VectorXd ImplicitEuler::compute_acceleration(const Eigen::VectorXd &v) const
	{
		return (v - v_prev()) / dt();
	}

	double ImplicitEuler::acceleration_scaling() const
	{
		return dt() * dt();
	}
} // namespace polyfem::time_integrator

#ifndef VIRTUAL_MECHANISM_GMR_H
#define VIRTUAL_MECHANISM_GMR_H

////////// VirtualMechanismInterface
#include <virtual_mechanism/virtual_mechanism_interface.h>

////////// Function Approximator
#include <functionapproximators/FunctionApproximatorGMR.hpp>
#include <functionapproximators/MetaParametersGMR.hpp>

////////// BOOST
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

namespace virtual_mechanism_gmr
{
  
  typedef DmpBbo::FunctionApproximatorGMR fa_t;

template <class VM_t>  
class VirtualMechanismGmr: public VM_t
{
	public:

	  VirtualMechanismGmr(int state_dim, boost::shared_ptr<fa_t> fa_pos_ptr, boost::shared_ptr<fa_t> fa_phase_dot_ptr);
	  
	  double getDistance(const Eigen::VectorXd& pos);
	  void setWeightedDist(const bool activate);
	  void getLocalKernel(Eigen::VectorXd& mean_variance) const;
	  double getProbability(const Eigen::VectorXd& pos);
	  
	protected:
	  
	  virtual void UpdateJacobian();
	  virtual void UpdateState();
	  virtual void ComputeInitialState();
	  virtual void ComputeFinalState();
	  void UpdateInvCov();
	  void ComputeStateGivenPhase(const double phase_in, Eigen::VectorXd& state_out, double& phase_dot_out);

	  boost::shared_ptr<fa_t> fa_pos_ptr_;
          boost::shared_ptr<fa_t> fa_phase_dot_ptr_;

	  Eigen::MatrixXd fa_input_;
	  Eigen::MatrixXd fa_pos_output_;
	  Eigen::MatrixXd fa_pos_output_dot_;
          Eigen::MatrixXd fa_phase_dot_output_;
	  Eigen::MatrixXd variance_pos_;
          Eigen::MatrixXd variance_phase_dot_;
	  Eigen::MatrixXd covariance_pos_;
	  Eigen::MatrixXd covariance_pos_inv_;
	  
	  Eigen::VectorXd err_;

	  double determinant_cov_;
	  double prob_;
	  bool use_weighted_dist_;
	  
	  /*virtual void AdaptGains(const Eigen::VectorXd& pos, const double dt);
	  Eigen::VectorXd normal_vector_;
	  Eigen::VectorXd prev_normal_vector_;
	  double max_std_variance_;
	  double K_max_;
	  double K_min_;
	  double std_variance_;
	  tool_box::MinJerk gain_adapter_;*/
};

}

#endif




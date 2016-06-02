#ifndef VIRTUAL_MECHANISM_GMR_H
#define VIRTUAL_MECHANISM_GMR_H

////////// VirtualMechanismInterface
#include <virtual_mechanism/virtual_mechanism_interface.h>

////////// Function Approximator
#include <functionapproximators/FunctionApproximatorGMR.hpp>
//#include <functionapproximators/MetaParametersGMR.hpp>
#include <functionapproximators/ModelParametersGMR.hpp>

////////// BOOST
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

////////// Toolbox
#include "toolbox/spline/spline.h"

namespace virtual_mechanism_gmr
{
  
  typedef DmpBbo::FunctionApproximatorGMR fa_t;

template <class VM_t>  
class VirtualMechanismGmr: public VM_t
{
	public:

      VirtualMechanismGmr(int state_dim, double K, double B, double Kf, double Bf, double fade_gain, const std::string file_path);
      ~VirtualMechanismGmr();

      virtual double getDistance(const Eigen::VectorXd& pos);
      virtual void setWeightedDist(const bool activate);
      virtual void getLocalKernel(Eigen::VectorXd& mean_variance) const;
      virtual double getGaussian(const Eigen::VectorXd& pos);
      void ComputeStateGivenPhase(const double abscisse_in, Eigen::VectorXd& state_out);
	  
	protected:
	  
      void CreateGmrFromTxt(const std::string file_path);
	  virtual void UpdateJacobian();
	  virtual void UpdateState();
	  virtual void ComputeInitialState();
	  virtual void ComputeFinalState();
	  void UpdateInvCov();

      //boost::shared_ptr<fa_t> fa_ptr_;
      fa_t* fa_; // Function Approximator

	  Eigen::MatrixXd fa_input_;
	  Eigen::MatrixXd fa_output_;
	  Eigen::MatrixXd fa_output_dot_;
	  Eigen::MatrixXd variance_;
	  Eigen::MatrixXd covariance_;
	  Eigen::MatrixXd covariance_inv_;
	  
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

template <typename VM_t>
class VirtualMechanismGmrNormalized: public VirtualMechanismGmr<VM_t>
{
    public:

      VirtualMechanismGmrNormalized(int state_dim, double K, double B, double Kf, double Bf, double fade_gain,  const std::string file_path);
      void ComputeStateGivenPhase(const double phase_in, Eigen::VectorXd& state_out, Eigen::VectorXd& state_out_dot, double& phase_out, double& phase_out_dot);

    protected:

      virtual void UpdateJacobian();
      virtual void UpdateState();
      virtual void UpdateStateDot();
      tk::spline spline_phase_;
      tk::spline spline_phase_inv_;
      std::vector<tk::spline > splines_xyz_;
      bool use_spline_xyz_;

      double z_;
      double z_dot_;
      double z_dot_ref_;

      Eigen::MatrixXd Jz_;

      long long loopCnt;
};

}

#endif




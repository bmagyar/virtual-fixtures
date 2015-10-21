#ifndef VIRTUAL_MECHANISM_INTERFACE_H
#define VIRTUAL_MECHANISM_INTERFACE_H

////////// Toolbox
#include <toolbox/toolbox.h>

////////// ROS
#include <ros/ros.h>

////////// Eigen
#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/Dense>
#include <boost/concept_check.hpp>
//#include <eigen3/Eigen/SVD>
//#include <eigen3/Eigen/Geometry>

////////// BOOST
//#include <boost/shared_ptr.hpp>
//#include <boost/make_shared.hpp>

#define LINE_CLAMP(x,y,x1,x2,y1,y2) do { y = (y2-y1)/(x2-x1) * (x-x1) + y1; } while (0)
	
namespace virtual_mechanism_interface 
{

class VirtualMechanismInterface
{
	public:
	  VirtualMechanismInterface(int expected_gmm_dim, double K, double B, double Kf):fa_dim_(expected_gmm_dim),ros_node_ptr_(NULL),phase_(0.0),
	  phase_prev_(0.0),phase_dot_(0.0),K_(K),B_(B),clamp_(1.0),adapt_gains_(false),Kf_(Kf),fade_(0.0),active_(false),move_forward_(true)
	  {
	      assert(fa_dim_ == 3 || fa_dim_ == 7); //xyz || xyz q
	      assert(K_ > 0.0);
	      assert(B_ > 0.0);
	      assert(Kf_ > 0.0);
	      
	      // Initialize the ros node
	      try
	      {
		ros_node_ptr_ = new tool_box::RosNode("virtual_mechanism"); // FIXME change here the name
	      }
	      catch(std::runtime_error err)
	      {
		std::cout<<err.what()<<std::endl;
	      }
	      
	      // Initialize/resize the attributes
	      // NOTE We assume that the phase has dim 1x1
	      position_dim_ = 3; // Always xyz
	      orientation_dim_ = 4; // Always quaternion
	      state_dim_ = position_dim_ + orientation_dim_; // Always pos + orientation
	      state_.resize(state_dim_);
	      //state_dot_.resize(state_dim);
	      position_.resize(position_dim_);
	      position_dot_.resize(position_dim_);
	      orientation_.resize(orientation_dim_);
	      torque_.resize(1);
	      force_.resize(position_dim_);
	      final_state_.resize(state_dim_);
	      initial_state_.resize(state_dim_);
	      J_.resize(position_dim_,1);
	      J_transp_.resize(1,position_dim_);
	      JxJt_.resize(1,1); // NOTE It is used to store the multiplication J * J_transp
	      
	      adaptive_gain_ptr_ = new tool_box::AdaptiveGain(Kf_,Kf_/2,0.1);

	      orientation_ << 0.0, 0.0, 0.0, 1.0; // Identity quaterion
	  }
	
	  virtual ~VirtualMechanismInterface()
	   {
	      if(ros_node_ptr_!=NULL)
		delete ros_node_ptr_;
	      if(adaptive_gain_ptr_!=NULL)
		delete adaptive_gain_ptr_;
	   }
	  
	  virtual void Update(Eigen::VectorXd& force, const double dt)
	  {
	    assert(dt > 0.0);
	    
	    // Save the previous phase
	    phase_prev_ = phase_;
  
	    // Update the Jacobian and its transpose
	    UpdateJacobian();
	    //J_transp_ = J_.transpose();
	    
	    // Update the phase
	    UpdatePhase(force,dt);
	    
	    // Saturate the phase if exceeds 1 or 0
	    ApplySaturation();
	    
	    // Compute the new state
	    UpdateState();
	    
	    // Compute the new position dot
	    //UpdateStateDot();
	    UpdatePositionDot();
	  }
	  
	  virtual void ApplySaturation()
	  {
	      // Saturations
	      if(phase_ > 1.0)
	      {
		//LINE_CLAMP(phase_,clamp_,0.9,1,1,0);
		phase_ = 1;
		//phase_dot_ = 0;
	      }
	      else if (phase_ < 0.0)
	      {
		//LINE_CLAMP(phase_,clamp_,0,0.1,0,1);
		phase_ = 0;
		//phase_dot_ = 0;
	      }
	  }
	  
	  inline void Update(const Eigen::VectorXd& pos, const Eigen::VectorXd& vel , const double dt, const double scale = 1.0)
	  {
	      assert(pos.size() == position_dim_);
	      assert(vel.size() == position_dim_);
	    
	      if(adapt_gains_) //FIXME
		AdaptGains(pos,dt);
	      
	      force_ = K_ * (position_ - pos);
	      force_ = force_ - B_ * vel;
	      force_ = scale * force_;
	      //force_ = scale * (K_ * (state_ - pos) - B_ * (vel));
	      
	      Update(force_,dt);
	  }
	  
	  virtual void AdaptGains(const Eigen::VectorXd& pos, const double dt){}
	  
	  virtual void getInitialPos(Eigen::VectorXd& state) const{assert(state.size() == state_dim_); state = initial_state_;};
	  virtual void getFinalPos(Eigen::VectorXd& state) const {assert(state.size() == state_dim_); state = final_state_;};
	  
	  inline void setAdaptGains(const bool adapt_gains) {adapt_gains_ = adapt_gains;}
	  inline void setActive(const bool active) {active_ = active;}
	  
	  inline void moveBackward() {move_forward_ = false;}
	  inline void moveForward() {move_forward_ = true;}
	  
	  inline double getTorque() const {return torque_(0,0);} // For test purpose
	  inline double getFade() const {return fade_;} // For test purpose
	  
	  inline double getPhaseDot() const {return phase_dot_;}
	  inline double getPhase() const {return phase_;}
	  inline void getState(Eigen::VectorXd& state) const {assert(state.size() == state_dim_); state = state_;}
	  //inline void getStateDot(Eigen::VectorXd& state_dot) const {assert(state_dot.size() == state_dim_); state_dot = state_dot_;}
	  inline void getPosition(Eigen::VectorXd& position) const {assert(position.size() == position_dim_); position = position_;}
	  inline void getPositionDot(Eigen::VectorXd& position_dot) const {assert(position_dot.size() == position_dim_); position_dot = position_dot_;}
	  inline void getOrientation(Eigen::VectorXd& orientation) const {assert(orientation.size() == orientation_dim_); orientation = orientation_;}
	  
	  inline double getK() const {return K_;}
	  inline double getB() const {return B_;}
	  inline void setK(const double& K){assert(K > 0.0); K_ = K;}
	  inline void setB(const double& B){assert(B > 0.0); B_ = B;}
	  
          inline void Init()
          {
              // Initialize the attributes
              UpdateJacobian();
              UpdateState();
              UpdatePositionDot();
	      ComputeInitialState();
	      ComputeFinalState();
          }
	  
	protected:
	    
	  virtual void UpdateJacobian()=0;
	  virtual void UpdateState()=0;
	  virtual void UpdatePosition()=0;
	  virtual void UpdateOrientation()=0;
	  virtual void UpdatePhase(const Eigen::VectorXd& force, const double dt)=0;
	  virtual void ComputeInitialState()=0;
	  virtual void ComputeFinalState()=0;
	  
	  /*inline void UpdateStateDot()
	  {
	      state_dot_ = J_ * phase_dot_;
	  }*/
	  
	  inline void UpdatePositionDot()
	  {
	      position_dot_ = J_ * phase_dot_;
	  }
	  
	  // Ros node
	  tool_box::RosNode* ros_node_ptr_;
	  
	  // States
	  double phase_;
	  double phase_prev_;
	  double phase_dot_;
	  int state_dim_;
	  int position_dim_;
	  int orientation_dim_;
	  Eigen::VectorXd state_;
	  //Eigen::VectorXd state_dot_;
	  Eigen::VectorXd position_;
	  Eigen::VectorXd position_dot_;
	  Eigen::VectorXd orientation_;
	  Eigen::VectorXd torque_;
	  Eigen::VectorXd force_;
	  Eigen::VectorXd initial_state_;
	  Eigen::VectorXd final_state_;
	  Eigen::MatrixXd JxJt_;
	  Eigen::MatrixXd J_;
	  Eigen::MatrixXd J_transp_;

	  // Gains
	  double B_;
	  double K_;
	  
	  // Clamping
	  double clamp_;
	  
	  // To activate the gain adapting
	  bool adapt_gains_;
	  
	  int fa_dim_;
	  
	  // Auto completion
	  double Kf_;
	  double fade_;
	  bool active_;
	  bool move_forward_;
	  tool_box::AdaptiveGain* adaptive_gain_ptr_;
};
  
class VirtualMechanismInterfaceFirstOrder : public VirtualMechanismInterface
{
	public:
	  //double K = 300, double B = 34.641016,
	  VirtualMechanismInterfaceFirstOrder(int expected_gmm_dim, double K = 700, double B = 52.91502622129181, double Kf = 1, double Bd_max = 1, double epsilon = 10):
	  VirtualMechanismInterface(expected_gmm_dim,K,B,Kf)
	  {
            assert(epsilon > 0.1);
            epsilon_ = epsilon;
	    Bd_ = 0.0;
	    Bd_max_ = Bd_max;
	    det_ = 1.0;
	    num_ = -1.0;
	  }

	protected:
	    
	  virtual void UpdateJacobian()=0;
	  virtual void UpdateState()=0;
	  virtual void ComputeInitialState()=0;
	  virtual void ComputeFinalState()=0;
	  
	  virtual void UpdatePhase(const Eigen::VectorXd& force, const double dt)
	  {
	    
	      assert(force.size() == position_dim_);
	    
	      JxJt_.noalias() = J_transp_ * J_;
	      
	      // Adapt Bf
	      Bd_ = std::exp(-4/epsilon_*JxJt_(0,0)) * Bd_max_; // NOTE: Since JxJt_ has dim 1x1 the determinant is the only value in it
	      //Bf_ = std::exp(-4/epsilon_*JxJt_.determinant()) * Bf_max_; // NOTE JxJt_.determinant() is always positive! so it's ok
	      
	      det_ = B_ * JxJt_(0,0) + Bd_ * Bd_;
	      
	      torque_.noalias() = J_transp_ * force;
	      
	      if(active_)
		  fade_ = 10 * (1 - fade_) * dt + fade_;
	      else
		  fade_ = 10 * (-fade_) * dt + fade_;

	      // Compute phase dot
	      if(move_forward_) // Go forward
	      {
		  Kf_ = adaptive_gain_ptr_->ComputeGain((1 - phase_));
		  phase_dot_ = (1-fade_) * num_/det_ * torque_(0,0) + fade_ * Kf_ * (1 - phase_);
	      }
	      else // Go back
	      {
		  Kf_ = adaptive_gain_ptr_->ComputeGain((0 - phase_));
		  phase_dot_ = (1-fade_) * num_/det_ * torque_(0,0) + fade_ * Kf_ * (0 - phase_);
	      }
	      // Compute the new phase
	      phase_ = phase_dot_ * dt + phase_prev_; // FIXME Switch to RungeKutta if possible
	  }
	  
	protected:
	  
	  // Tmp variables
	  double det_;
	  double num_; 
	  
	  double Bd_; // Damp term
	  double Bd_max_; // Max damp term
	  double epsilon_;
	  
};

class VirtualMechanismInterfaceSecondOrder : public VirtualMechanismInterface
{
	public:
	  //double K = 300, double B = 34.641016, double K = 700, double B = 52.91502622129181, 900 60, 800 56.568542494923804
	  VirtualMechanismInterfaceSecondOrder(int expected_gmm_dim, double K = 700, double B = 52.91502622129181, double Kf = 20, double Bf = 8.94427190999916):
	  VirtualMechanismInterface(expected_gmm_dim,K,B,Kf)
	  {
	      
	      assert(Bf > 0.0);
	      
	      Bf_ = Bf;
	      phase_dot_prev_ = 0.0;
	      phase_ddot_ = 0.0;
	      
	      // Resize the attributes
	      phase_state_dot_.resize(2); //phase_dot and phase_ddot
	      phase_state_.resize(2); //phase_ and phase_dot
	      phase_state_integrated_.resize(2);

	      k1_.resize(2);
	      k2_.resize(2);
	      k3_.resize(2);
	      k4_.resize(2);
	      
	      k1_.fill(0.0);
              k2_.fill(0.0);
	      k3_.fill(0.0);
	      k4_.fill(0.0);
              
	  }
	
	  virtual ~VirtualMechanismInterfaceSecondOrder()
	  {
	    
	  }
	  
	  using VirtualMechanismInterface::Update; // Use the VirtualMechanismInterface overloaded function
	  
	  virtual void Update(Eigen::VectorXd& force, const double dt)
	  {
	    
	    phase_dot_prev_ = phase_dot_;
	    VirtualMechanismInterface::Update(force,dt);
	    
	  }
	  
	  virtual void ApplySaturation()
	  {
	      // Saturations
	      if(phase_ > 1.0)
	      {
		//LINE_CLAMP(phase_,clamp_,0.9,1,1,0);
		phase_ = 1;
		phase_dot_ = 0;
	      }
	      else if (phase_ < 0.0)
	      {
		//LINE_CLAMP(phase_,clamp_,0,0.1,0,1);
		phase_ = 0;
		phase_dot_ = 0;
	      }
	  }
	  
	  inline double getPhaseDDot() const {return phase_ddot_;}

	protected:
	    
	  virtual void UpdateJacobian()=0;
	  virtual void UpdateState()=0;
	  virtual void ComputeInitialState()=0;
	  virtual void ComputeFinalState()=0;

	  void IntegrateStepRungeKutta(const double& dt, const double& input, const Eigen::VectorXd& phase_state, Eigen::VectorXd& phase_state_integrated)
	  {
	 
	    phase_state_integrated = phase_state;
	    
	    DynSystem(dt,input,phase_state_integrated);
	    k1_ = phase_state_dot_;
	    
	    phase_state_integrated = phase_state_ + 0.5*dt*k1_;
	    DynSystem(dt,input,phase_state_integrated);
	    k2_ = phase_state_dot_;
	    
	    phase_state_integrated = phase_state_ + 0.5*dt*k2_;
	    DynSystem(dt,input,phase_state_integrated);
	    k3_ = phase_state_dot_;
	    
	    phase_state_integrated = phase_state_ + dt*k3_;
	    DynSystem(dt,input,phase_state_integrated);
	    k4_ = phase_state_dot_;
	    
	    phase_state_integrated = phase_state_ + dt*(k1_ + 2.0*(k2_+k3_) + k4_)/6.0;
	  
	  }
	  
	  inline void DynSystem(const double& dt, const double& input, const Eigen::VectorXd& phase_state)
	  {
	     if(active_)
	     {
		fade_ = 10 * (1 - fade_) * dt + fade_;
		//phase_state_dot_(1) = - B_ * JxJt_(0,0) * phase_state(1) - input + fade_ * (- Bf_ * phase_state(1) + Kf_ * (1 - phase_state(0)));
		//phase_ddot_ = - B_ * JxJt_(0,0) * phase_dot_ - torque_(0,0) - Bf_ * phase_dot_ + Kf_ * (1 - phase_);
	     }
	     else
	     {
		fade_ = 10 * (-fade_) * dt + fade_;
		//phase_state_dot_(1) = - B_ * JxJt_(0,0) * phase_state(1) - input + fade_ * (- Bf_ * phase_state(1) + Kf_ * (1 - phase_state(0)));;
		//phase_ddot_ = - B_ * JxJt_(0,0) * phase_dot_ - torque_(0,0);
	     }
	     Kf_ = adaptive_gain_ptr_->ComputeGain((1 - phase_state(0)));
	     phase_state_dot_(1) = 10*( - B_ * JxJt_(0,0) * phase_state(1) - input + fade_ * (- Bf_ * phase_state(1) + Kf_ * (1 - phase_state(0))) );
	     phase_state_dot_(0) = phase_state(1);
	  }
	  
	  virtual void UpdatePhase(const Eigen::VectorXd& force, const double dt)
	  {
	      JxJt_.noalias() = J_transp_ * J_;
	    
	      torque_.noalias() = J_transp_ * force;

	      phase_state_(0) = phase_;
	      phase_state_(1) = phase_dot_;
	      
	      DynSystem(dt,torque_(0),phase_state_);
	      
	      IntegrateStepRungeKutta(dt,torque_(0),phase_state_,phase_state_integrated_);
	      
	      phase_ = phase_state_integrated_(0);
	      phase_dot_ = phase_state_integrated_(1);
	      phase_ddot_ = phase_state_dot_(1);
	      
	      //DynSystem(const Eigen::VectorXd& phase_state, const double& dt, const double& input);
	      
	      /*if(active_)
	        phase_state_dot_(1) = - B_ * JxJt_(0,0) * phase_state(1) - torque_(0,0) - Bf_ * phase_state(1) + Kf_ * (1 - phase_state(0));
		//phase_ddot_ = - B_ * JxJt_(0,0) * phase_dot_ - torque_(0,0) - Bf_ * phase_dot_ + Kf_ * (1 - phase_);
	      else
		phase_state_dot_(1) = - B_ * JxJt_(0,0) * phase_state(1) - torque_(0,0);
		//phase_ddot_ = - B_ * JxJt_(0,0) * phase_dot_ - torque_(0,0);
	      
	      phase_state_dot_(0) = phase_state(1);*/

	      // Compute the new phase
	      // FIXME Switch to RungeKutta  
	      //phase_dot_ = phase_ddot_ * dt + phase_dot_prev_; 
	      //phase_ = phase_dot_ * dt + phase_prev_;
	  }
	  
	  double phase_dot_prev_;
	  double phase_ddot_;

	  Eigen::VectorXd phase_state_;
	  Eigen::VectorXd phase_state_dot_;
	  Eigen::VectorXd phase_state_integrated_;
	  Eigen::VectorXd state_dot_;
 
	  Eigen::VectorXd k1_, k2_, k3_, k4_;

	  // Fade system variables
	  double Bf_;

};

}

#endif




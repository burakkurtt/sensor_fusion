#include <kalman_filter/KalmanOrientation.h>
#include <sstream>
// a 10-state Kalman Filter to fuse monocular vision position with IMU
// Visual scale  can be recovered as well.

  //________________________
  // Constructor
  KalmanOrientation::KalmanOrientation(double MNstd, double PNstd)//:
	//delayed_imu_readings_(2), // vision has about 0.01ms delay, 180Hz imu update (0.0056), 0.01*180 = 1.8
	//init_lambda(2.0),
	//init_lambda_uncer(2.0), //1.5
  //      reset_exposure(0),
	//T(1.0/180.0) // imu update time interval
  {
    // A
    A  = Matrix<double, 4, 4>::Identity();
    //Amat.block<3,3>(0,3) = T * Matrix3d::Identity();
    
    // Q
    Q = Matrix<double, 4, 4>::Identity();
    Q = PNstd * PNstd * Q;
    //Matrix<double,4,4> Q = Matrix<double,7,7>::Zero();
    //Q.block<3,3>(0,0) = n_s*n_s*Matrix3d::Identity();
    //Q.block<3,3>(3,3) = n_b*n_b*Matrix3d::Identity();
    //Q(6,6) = n_lamda*n_lamda;
    
    // R
    R = Matrix<double, 4, 4>::Identity();
    R = MNstd * MNstd * R;

    // W, WQWT
    //Matrix<double, 10, 7> W = Matrix<double, 10,  7>::Zero();
    //W.block<3,3>(3,0) = T * Matrix3d::Identity();
    //W.block<4,4>(6,3) = Matrix<double,4,4>::Identity();
    //WQWT  = W*Q*W.transpose();
    
    // Hv, KGain
    //Hv    = Matrix<double,  3, 10>::Zero();
    //KGain = Matrix<double, 10,  3>::Zero();
    H = Matrix<double, 4, 4>::Identity();
    K = Matrix<double, 4, 4>::Identity();

    P = Matrix<double,4,4>::Zero();
    state_q << 1, 0, 0, 0;
 
    // Reset kalman state
    //resetLeft = 0;
    //resetLeft = 1;
    //toReset = true;
    //resetKalman();
    //reset_loop = 15; // 20 continuous measurements before using the vision data
    //resetLeft = reset_loop;
  }

  void KalmanOrientation::fullkalmanfilter(const Vector4d& q_sensor)
  {
    // time update
    state_q_ = A * state_q;
    P_ = A * P * A.transpose() + Q;

    // Measurement Update
    Matrix4d temp = H * P_ * H.transpose() + R;
    K = P_ * H.transpose() * temp.inverse();

    estError_ = q_sensor - (H * state_q_);
    state_correction = K * estError_;
    state_q = state_q_ + state_correction;

    P = (Matrix<double,4,4>::Identity() - (K * H)) * P_;
  }

  //________________________
  // Reset Kalman filter to initial state
  /*
  void KalmanFilter10::resetKalman(Vector3d pInit)
  {
    std::cout << resetLeft << std::endl;
    if (resetLeft==0) { // this allows to skip several vision measurements at initailisation
	// for robust reset
        toReset = false;
        resetLeft = reset_loop;
	
    } else {
	// for robust reset
	--resetLeft;
	
	// Init State Vector
        state = Matrix<double, 10,  1>::Zero();
        state.head<3>() = pInit;
        state(9) = init_lambda;
        //std::cout << init_lambda << std::endl;

        // Init State/process Propability Matrix (assumes large initial uncertainty)
	Matrix<double, 10, 1> vecTwo; vecTwo << 1.0,1.0,1.0, 1.0,1.0,1.0, 0.5,0.5,0.5, init_lambda_uncer; //5 2 0.1 3
        Pmat  = vecTwo * vecTwo.transpose();
	
	// clear imu buffers
	accel_bf.clear();
	Rot_bf.clear();
	q_bf.clear();
	
	// reset lambda init count
	lambda_ini_count = 10;
	
	// estimation error
	estError_ << 100.0, 100.0, 100.0;
    }
  }


  //_________________________
  // State process update, when new sensor measurement is available
  void KalmanFilter10::StateUptSen(const Vector3d& accel, const Matrix3d& Rot, double msgTime)
  {
    // insert to buffer
    Rot_bf.push_back(Rot);
    accel_bf.push_back(accel);
 
    if (Rot_bf.size() == (delayed_imu_readings_+1)) {// num of delayed measurements + 1
      //Update time interval
      double dt = msgTime - imu_timer_;   // imu_timer (ValidationGuard::imu_Cb)
      if (dt < 0.05 && dt > 0.0) {
        T = 199.0/200.0 * T + (dt / 200.0); // update the time interval (average over 200 msgs)
      }
      imu_timer_ = msgTime;

      // Update A
      Amat.block<3,3>(0,3) = T * Matrix3d::Identity();
      //Amat.block<3,3>(3,6) = T * Rot_bf.front();
      Amat.block<3,3>(3,6) = T * Matrix3d::Identity(); 

      // state Means (xnew = f(x))
      state.block<3,1>(3,0) += Amat.block<3,3>(3,6)*state.block<3,1>(6,0) + T*accel_bf.front();
      state.block<3,1>(0,0) += T * state.block<3,1>(3,0);

      // debug
      //std::cout << "[state vector 0 - 5] " << state[0] << " " << state[1] << " " << state[2] << " " << state[3] << " " << state[4] << " " << state[5] << std::endl;  
    
      //_____________________________
      // state Probability distribution
      Pmat = Amat * Pmat * Amat.transpose() + WQWT;
    
      // delet old buffer elements
      Rot_bf.pop_front();
      accel_bf.pop_front();
    }
  }




  //_________________________
  // Filter vision measurement update
  void KalmanFilter10::MeasureUptVis(const Vector3d& z, double n_v)
  {
    //std::cout << " 11 " << std::endl;
    if (toReset) {
      resetKalman(z/init_lambda);
    } else if (Rot_bf.size() == delayed_imu_readings_) { // ensure imu data comes first
      Hv.block<3,3>(0,0) = state(9) * Matrix3d::Identity();
      Hv.col(9) = state.head<3>();
      VRVT  = n_v*n_v*Matrix3d::Identity();

      Matrix3d temp = Hv * Pmat * Hv.transpose() + VRVT;
      KGain  = Pmat * Hv.transpose() * temp.inverse();
      estError_ = z - (state(9)*state.head<3>());
      state_correction = KGain * estError_;
      state += state_correction;
      Pmat   = (Matrix<double,10,10>::Identity() - (KGain * Hv)) * Pmat;
      
      if (lambda_ini_count >0){
        state(9)  = init_lambda;
	      Pmat(9,9) = init_lambda_uncer * init_lambda_uncer;
        lambda_ini_count--;
      }
    }
  }
  */





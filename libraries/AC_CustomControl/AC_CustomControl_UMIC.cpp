#include "AC_CustomControl_UMIC.h"

#if CUSTOMCONTROL_UMIC_ENABLED

#include <GCS_MAVLink/GCS.h>

// table of user settable parameters
const AP_Param::GroupInfo AC_CustomControl_UMIC::var_info[] = {
    // @Param: PARAM0
    // @DisplayName: UMIC param0
    // @Description: Dummy parameter for empty custom controller backend
    // @User: Advanced
    AP_GROUPINFO("PARAM0", 1, AC_CustomControl_UMIC, param0, 0.0f),

    AP_GROUPEND
};

// initialize in the constructor
AC_CustomControl_UMIC::AC_CustomControl_UMIC(AC_CustomControl& frontend, AP_AHRS_View*& ahrs, AC_AttitudeControl*& att_control, AP_MotorsMulticopter*& motors, float dt) :
    AC_CustomControl_Backend(frontend, ahrs, att_control, motors, dt)
{
    AP_Param::setup_object_defaults(this, var_info);
    simulink_controller.initialize();
}

// update controller
// return roll, pitch, yaw controller output
Vector3f AC_CustomControl_UMIC::update(void)
{
    // reset controller based on spool state
    switch (_motors->get_spool_state()) {
        case AP_Motors::SpoolState::SHUT_DOWN:
        case AP_Motors::SpoolState::GROUND_IDLE:
            // We are still at the ground. Reset custom controller to avoid
            // build up, ex: integrator
            reset();
            break;

        case AP_Motors::SpoolState::THROTTLE_UNLIMITED:
        case AP_Motors::SpoolState::SPOOLING_UP:
        case AP_Motors::SpoolState::SPOOLING_DOWN:
            // we are off the ground
            break;
    }

    // run custom controller after here
    Quaternion attitude_body, attitude_target;
    _ahrs->get_quat_body_to_ned(attitude_body);

    attitude_target = _att_control->get_attitude_target_quat();
    // This vector represents the angular error to rotate the thrust vector using x and y and heading using z
    Vector3f attitude_error;
    float _thrust_angle, _thrust_error_angle;
    _att_control->thrust_heading_rotation_angles(attitude_target, attitude_body, attitude_error, _thrust_angle, _thrust_error_angle);

    // recalculate ang vel feedforward from attitude target model
    // rotation from the target frame to the body frame
    Quaternion rotation_target_to_body = attitude_body.inverse() * attitude_target;
    // target angle velocity vector in the body frame
    Vector3f ang_vel_body_feedforward = rotation_target_to_body * _att_control->get_attitude_target_ang_vel();
    Vector3f gyro_latest = _ahrs->get_gyro_latest();

    float arg_attitude_error[3]{attitude_error.x, attitude_error.y, attitude_error.z};
    float arg_rate_ff[3]{ang_vel_body_feedforward.x, ang_vel_body_feedforward.y, ang_vel_body_feedforward.z};
    float arg_rate_meas[3]{gyro_latest.x, gyro_latest.y, gyro_latest.z};
    float arg_CtrlOUT[3];
    
    simulink_controller.step(arg_attitude_error, arg_rate_ff, arg_rate_meas, arg_CtrlOUT);

    return Vector3f(arg_CtrlOUT[0], arg_CtrlOUT[1], arg_CtrlOUT[2]);
    //return Vector3f(0.0, 0.0, 0.0);
}

// reset controller to avoid build up on the ground
// or to provide bumpless transfer from arducopter main controller
void AC_CustomControl_UMIC::reset(void){}

#endif

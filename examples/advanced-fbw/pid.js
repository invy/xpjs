
const MathHelper = imports.mathhelper;

function PID(Kp, Ki, Kd, iLimit) {
  this.Kp = Kp;
  this.Ki = Ki;
  this.Kd = Kd;
  this.iLimit = iLimit;

  this.integral = 0;
  this.previousError = 0;
  this.bias = 0;

  this.update = function(dt, setValue, measuredValue) {
    var error = setValue - measuredValue;
    this.intgral += error*dt;
    if(iLimit > 0)
      this.integral = MathHelper.limitAbs(this.integral, iLimit);
    var derivative = (error - this.previousError)/dt;
    this.previousError = error;
    return this.Kp*error + this.Ki*this.integral + this.Kd*derivative;
  }
}

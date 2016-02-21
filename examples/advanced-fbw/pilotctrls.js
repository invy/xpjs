

const axisValues = requestDRef('sim/joystick/joystick_axis_values');


/*
 * challs characterizes pilot inputs, such as pitch, roll and yaw
 */
function PilotCtrls(r, p, y) {
  this.rollAxis = r;
  this.pitchAxis = p;
  this.yawAxis = y;
  this.rollInput = 0;
  this.pitchInput = 0;
  this.yawInput = 0;
  this.neutralZone = 0.03;

  this.update = function() {
    this.pitchInput = getAt(axisValues, this.pitchAxis);
    this.rollInput = getAt(axisValues, this.rollAxis);
    this.yawInput = getAt(axisValues, this.yawAxis);
  }
  this.isNeutral = function() {
    if(Math.abs(this.pitchInput - 0.5) < this.neutralZone &&
       Math.abs(this.rollInput - 0.5) < this.neutralZone)
    {
      return true;
    }
    return false;
  }
}
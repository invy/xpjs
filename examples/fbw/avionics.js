
xplog('plugin started at: ' +new Date())

// data ref definition
var axisAssignments = requestDRef('sim/joystick/joystick_axis_assignments');
var axisValues = requestDRef('sim/joystick/joystick_axis_values');
var timerDataRef = requestDRef('sim/time/total_running_time_sec');
var gNrmlDataRef = requestDRef('sim/flightmodel/forces/g_nrml');

var pitchDataRef = requestDRef('sim/cockpit2/gauges/indicators/pitch_electric_deg_pilot');
var rollDataRef = requestDRef('sim/cockpit2/gauges/indicators/roll_electric_deg_pilot');


var hstabElev = requestDRef('sim/flightmodel2/wing/elevator1_deg');
var ail1 = requestDRef('sim/flightmodel2/wing/aileron1_deg');

var elevatorTrim = requestDRef('sim/flightmodel2/controls/elevator_trim');
/*var elevTrimDref = requestDRef('sim/flightmodel2/controls/stabilizer_deflection_degrees');
var elevTrimUpMaxDref = requestDRef('sim/aircraft/controls/acf_hstb_trim_up');
var elevTrimDnMaxDref = requestDRef('sim/aircraft/controls/acf_hstb_trim_dn');*/

var gndRef = requestDRef('sim/flightmodel/failures/onground_any');

var overrideSurfaces = requestDRef('sim/operation/override/override_control_surfaces');
var overrideFlightControl = requestDRef('sim/operation/override/override_flightcontrol');

// init script values
var pitchAxis = axisAssignments[2];
var prevTime = get(timerDataRef);
var curTime = 0;


var PID = function pid(Kp, Ki, Kd) {
  this.Kp = Kp;
  this.Ki = Ki;
  this.Kd = Kd;
  this.integral = 0;
  this.previousError = 0;
  this.bias = 0;

  this.update = function(dt, setValue, measuredValue) {
    var error = setValue - measuredValue;
		this.intgral += error*dt;
    var derivative = (error - this.previousError)/dt;
    this.previousError = error;
    return this.Kp*error + this.Ki*this.integral + this.Kd*derivative;
  }

}

/*
 * challs characterizes pilot inputs, such as pitch, roll and yaw
 */
var PilotCtrls = function PilotCtrls(r, p, y) {
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

/*
 * Control surfaces assignment
 */
var CtrlSurfacesAssignment = function CtrlSurfacesAssignment(aLeft, aRight, eLeft, eRight, r) {
  this.aileronLeft = aLeft;
  this.aileronRight = aRight;
  this.elevatorLeft = eLeft;
  this.elevatorRight = eRight;
  this.ruddder = r;
}

/*
 * Control surfaces
 */
var CtrlSurfaces = function CtrlSurfaces(elevMax, ailrMax, ruddMax, a) {
  this.elevDeg = 0;
  this.ailrDeg = 0;
  this.ruddDeg = 0;
  this.elevMax = elevMax;
  this.ailrMax = ailrMax;
  this.ruddMax = ruddMax;

  this.surfA = a;


  this.update = function() {
    setAt(hstabElev, this.surfA.elevatorRight, this.elevDeg);
    setAt(hstabElev, this.surfA.elevatorLeft, this.elevDeg);
    setAt(ail1, this.surfA.aileronRight, -this.ailrDeg);
    setAt(ail1, this.surfA.aileronLeft, this.ailrDeg);
  }
  this.setAilrDeg = function(deg) {
    if(deg > this.ailrMax)
      deg = this.ailrMax;
    else if(-deg < -this.ailtMax)
      deg = -this.ailrMax;
    this.ailrDeg = deg;
  }
  this.setElevDeg = function(deg) {
    var neg = deg < 0 ? true : false;
    if(Math.abs(deg) >= this.elevMax) {
      if(neg)
        this.elevDeg = -this.elevMax;
      else
        this.elevDeg = this.elevMax;
    }
    else
      this.elevDeg = deg;
  }
  this.adjustElevatorTrim = function(dt) {
    var currentTrim = get(elevatorTrim);
    // nose up trim
    if(this.elevDeg < 0 && currentTrim < 0.95) {
      set(elevatorTrim, currentTrim + 0.3*dt);
    }
    // nose down trim
    else if(this.elevDeg > 0 && currentTrim > -0.95) {
      set(elevatorTrim, currentTrim - 0.3*dt);
    }
  }
}

/*
 * Flight Contorl Computer aka Fly-by-Wire
 *  Implements: Autotrim, holding orientation, flight envelope (todo)
 */
var FCC = function FCC(pctl, surfaces) {
  this.modes = {
    GROUND : 1,
    FLIGHT : 2
  };
  this.mode = this.modes.GROUND;

  this.pInput = pctl;
  this.surfaces = surfaces;

  this.pidG = new PID(1.0, 1.0, 1.0);
  this.pidPitch = new PID(1.0, 1.0, 1.0);
  this.pidRoll = new PID(1.0, 1.0, 1.0);

  this.dt = 0.0;

  this.setPitch = 0.0;
  this.setRoll = 0.0;

  this.wasNeutral = false;

  this.update = function(dt) {
    this.dt = dt;
    this.pInput.update();
    this.detectMode();
    if(this.mode == this.modes.GROUND)
      this.direct();
    else
      this.normalLaw();
    this.surfaces.update();
  }
  this.direct = function() {
    this.surfaces.setElevDeg(-(this.pInput.pitchInput-0.5)*this.surfaces.elevMax);
    this.surfaces.setAilrDeg((this.pInput.rollInput-0.5)*this.surfaces.ailrMax);
  }
  this.normalLaw = function() {
    if(this.pInput.isNeutral()) {
      if(this.wasNeutral != true) {
        this.rememberOrientation();
        this.wasNeutral = true;
        xplog('input neutral, remember orientation: pitch: ' + this.setPitch + ' and rull: ' + this.setRoll);
      }
      this.holdOrientation();
    }
    else {
      // for now use direct law, side-stick in airbus controls G load in normal law
      this.direct();
      this.wasNeutral = false;
    }
  }
  this.holdG = function() {
    var messG = get(gNrmlDataRef);
    var setG = 1 + (this.pInput.pitchInput-0.5)*1.5;
    var out = this.pidG.update(this.dt, setG, messG);
    this.surfaces.setElevDeg(-(out*0.01)*this.surfaces.elevMax);
    this.surfaces.setAilrDeg((this.pInput.rollInput-0.5)*this.surfaces.ailrMax);
  }
  this.holdOrientation = function() {
    var messPitch = get(pitchDataRef);
    var messRoll = get(rollDataRef);
    var outPitch = this.pidPitch.update(this.dt, this.setPitch, messPitch);
    var outRoll = this.pidRoll.update(this.dt, this.setRoll, messRoll);

    this.surfaces.setElevDeg(-(outPitch*0.1)*this.surfaces.elevMax);
    this.surfaces.setAilrDeg((outRoll*0.1)*this.surfaces.ailrMax);
    this.surfaces.adjustElevatorTrim(this.dt);
  }
  this.rememberOrientation = function() {
    this.setPitch = get(pitchDataRef);
    this.setRoll = get(rollDataRef);
  }

  this.auto = function() {
  }
  this.detectMode = function() {
    if(get(gndRef) != 1)
      this.mode = this.modes.FLIGHT;
    else
      this.mode = this.modes.GROUND;
  }
}

// Init flight controls, pilot input and fcc (fbw)
var pctl = new PilotCtrls(0, 1, 2);
var sa = new CtrlSurfacesAssignment(2, 1, 8, 9, 0);
var surfaces = new CtrlSurfaces(30.0, 30.0, 30.0, sa);

var fcc = new FCC(pctl, surfaces);

set(overrideSurfaces, 1);
set(overrideFlightControl, 1);

function update() {

  curTime = get(timerDataRef);
  var dt = curTime-prevTime;
  prevTime = curTime;
  fcc.update(dt);
}



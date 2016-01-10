
xplog('plugin started at: ' +new Date())

// data ref definition
const axisAssignments = requestDRef('sim/joystick/joystick_axis_assignments');
const axisValues = requestDRef('sim/joystick/joystick_axis_values');
const timerDataRef = requestDRef('sim/time/total_running_time_sec');
const gNrmlDataRef = requestDRef('sim/flightmodel/forces/g_nrml');

const pitchDataRef = requestDRef('sim/cockpit2/gauges/indicators/pitch_electric_deg_pilot');
const rollDataRef = requestDRef('sim/cockpit2/gauges/indicators/roll_electric_deg_pilot');

const RADataRef = requestDRef('sim/cockpit2/gauges/indicators/radio_altimeter_height_ft_pilot');

const hstabElev = requestDRef('sim/flightmodel2/wing/elevator1_deg');
const ail1 = requestDRef('sim/flightmodel2/wing/aileron1_deg');

const elevatorTrim = requestDRef('sim/flightmodel2/controls/elevator_trim');
//var elevTrimDref = requestDRef('sim/flightmodel2/controls/stabilizer_deflection_degrees');
const elevTrimUpMaxDref = requestDRef('sim/aircraft/controls/acf_hstb_trim_up');
const elevTrimDnMaxDref = requestDRef('sim/aircraft/controls/acf_hstb_trim_dn');

const flapsHandleDeployRatio = requestDRef('sim/flightmodel2/controls/flap_handle_deploy_ratio');
const flapsDeployRatioDRef = requestDRef('sim/flightmodel2/controls/flap1_deploy_ratio');
const lWingFlapsAngle = requestDRef('sim/flightmodel/controls/wing1l_fla1def');
const rWingFlapsAngle = requestDRef('sim/flightmodel/controls/wing1r_fla1def');
//const slatsRat = requestDRef('sim/flightmodel/controls/slatrat');
const slatsRat = requestDRef('sim/flightmodel2/controls/slat1_deploy_ratio');

const gndRef = requestDRef('sim/flightmodel/failures/onground_any');

const overrideSurfaces = requestDRef('sim/operation/override/override_control_surfaces');
const overrideFlightControl = requestDRef('sim/operation/override/override_flightcontrol');

// init script values
var pitchAxis = axisAssignments[2];
var prevTime = get(timerDataRef);
var curTime = 0;

function limit(v, l) {
  if(v > l)
    return l;
  return v;
}

function limitAbs(v, l) {
  if(v > l)
    return l;
  if(-v < -l)
    return -l;
  return v;
}

function limitMinMax(v, mi, ma) {
  if(v > ma)
    return ma;
  if(v < mi)
    return mi;
  return v;
}

function areEqual(deg1, deg2) {
  if(deg1 > 0 && deg2 > 0 && Math.abs(deg1 - deg2) < 1e-02 ||
     deg1 < 0 && deg2 < 0 && Math.abs(-deg1 + deg2) < 1e-02)
    return true;
  return false;
}


var PID = function pid(Kp, Ki, Kd, iLimit) {
  this.Kp = Kp;
  this.Ki = Ki;
  this.Kd = Kd;
  this.integral = 0;
  this.previousError = 0;
  this.bias = 0;
  this.iLimit = iLimit;

  this.update = function(dt, setValue, measuredValue) {
    var error = setValue - measuredValue;
    this.intgral += error*dt;
    if(iLimit > 0)
      this.integral = limitAbs(this.integral, iLimit);
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
  var Aileron = function Aileron(aMax, assign, defDirection) {
    this.ailrDeg = 0;
    this.ailrMax = aMax;
    this.ailrDefRate = 50;
    this.assign = assign;
    this.defDirection = defDirection;
    this.update = function() {
      setAt(ail1, assign, this.ailrDeg*this.defDirection);
    }
    this.setAilrDeg = function(deg) {
      this.ailrDeg = limitMinMax(deg, -this.ailrMax, this.ailrMax);
    }
    this.setAilrDeg = function(dt, deg) {
//    if(this.areEqual(deg, this.ailrDeg))
//      return;
      if(this.ailrDeg < deg) {
        this.ailrDeg += Math.min(this.ailrDefRate * dt, deg - this.ailrDeg);
      }
      else if(this.ailrDeg > deg) {
        this.ailrDeg -= Math.min(this.ailrDefRate * dt, this.ailrDeg - deg);
      }
      this.ailrDeg = limitMinMax(this.ailrDeg, -this.ailrMax, this.ailrMax);
    }
  }
  var Elevator = function Elevator(aMax, assignLeft, assignRight) {
    this.elevDeg = 0;
    this.elevMax = aMax;
    this.elevDefRate = 50;
    this.assignLeft = assignLeft;
    this.assignRight = assignRight;
    this.update = function() {
      setAt(hstabElev, assignLeft, this.elevDeg);
      setAt(hstabElev, assignRight, this.elevDeg);
   }
    this.setElevDeg = function(deg) {
      this.elevDeg = limitMinMax(deg, -this.elevMax, this.elevMax);
    }
    this.setElevDeg = function(dt, deg) {
//    if(this.areEqual(deg, this.elevDeg))
//      return;
      if(this.elevDeg < deg)
        this.elevDeg += Math.min(this.elevDefRate * dt, deg - this.elevDeg);
      else if(this.elevDeg > deg)
        this.elevDeg -= Math.min(this.elevDefRate * dt, this.elevDeg - deg);
      this.elevDeg = limitMinMax(this.elevDeg, -this.elevMax, this.elevMax);
    }
  }
  var Rudder = function Rudder(rMax) {
    this.ruddDeg = 0;
    this.ruddMax = rMax;
    this.ruddDefRate = 50; // should be function of speed
    this.update = function() {
    }
  }
  var Flaps = function Flaps() {
    this.isCleanConfig = function() {
      if(get(flapsDeployRatioDRef) == 0.0)
        return true;
      return false;
    }
    this.update = function() {
      const hdr = get(flapsHandleDeployRatio);
      set(flapsDeployRatioDRef, hdr);
      set(lWingFlapsAngle, hdr*40);
      set(rWingFlapsAngle, hdr*40);
    }
  }
  var Slats = function Slats() {
    this.isCleanConfig = function() {
      if(get(flapsDeployRatioDRef) == 0.0)
        return true;
      return false;
    }
    this.update = function(dt) {
      const slatPos = get(slatsRat);
      if(get(flapsHandleDeployRatio) > 0 && slatPos < 1) {
        var newPos = Math.min(slatPos+0.3*dt, 1); 
        set(slatsRat, newPos);
      }
      else if(get(flapsHandleDeployRatio) == 0 && slatPos > 0) {
        var newPos = Math.max(slatPos-0.3*dt, 0); 
        set(slatsRat, newPos);
      }
    }
  }
  var Spoilers = function Spoilers() {
  }
  var Stab = function Stab() {
    this.stabUpMax = get(elevTrimUpMaxDref);
    this.stabDnMax = get(elevTrimDnMaxDref);
    this.stabMax = Math.abs(this.stabUpMax) + Math.abs(this.stabDnMax);
    // deflection rate in deg/sec
    this.stabDefRateDeg = 2; // 2 deg/sec
    this.stabDefRateFra = this.stabDefRateDeg/this.stabMax;
    this.update = function() {
    }
  }

  this.surfA = a;

  this.lAil = new Aileron(ailrMax, this.surfA.aileronLeft, 1);
  this.rAil = new Aileron(ailrMax, this.surfA.aileronRight, -1);
  
  this.elev = new Elevator(elevMax, this.surfA.elevatorLeft, this.surfA.elevatorRight);
  this.rudd = new Rudder(ruddMax);
  this.stab = new Stab();
  this.flaps = new Flaps();
  this.slats = new Slats();

  this.update = function(dt) {
    this.lAil.update();
    this.rAil.update();
    this.elev.update();
    this.rudd.update();
    this.stab.update();
    this.flaps.update();
    this.slats.update(dt);
  }
  this.setAilrDefFrac = function(dt, frac) {
    this.lAil.setAilrDeg(dt, frac*this.lAil.ailrMax*2);
    this.rAil.setAilrDeg(dt, frac*this.rAil.ailrMax*2);
  }
  this.setElevFrac = function(dt, frac) {
    this.elev.setElevDeg(dt, frac*this.elev.elevMax*2);
  }
  this.adjustElevatorTrim = function(dt) {
    var currentTrim = get(elevatorTrim);
    // nose up trim
    if(this.elevDeg < 0 && currentTrim < 0.95) {
      set(elevatorTrim, currentTrim + this.stabDefRateFra*dt);
    }
    // nose down trim
    else if(this.elevDeg > 0 && currentTrim > -0.95) {
      set(elevatorTrim, currentTrim - this.stabDefRateFra*dt);
    }
  }

  this.isCleanConfig = function() {
    if(this.flaps.isCleanConfig() && this.slats.isCleanConfig())
      return true;
    return false;
  }


}

/*
 * Flight Contorl Computer aka Fly-by-Wire
 *  Implements: Autotrim, holding attitude, flight envelope (todo)
 */
var FCC = function FCC(pctl, surfaces) {
  var ControlMode = function ControlMode() {
    this.modes = {
      GROUND : 1,
      FLIGHT : 2,
      FLARE : 3
    };
    this.mode = this.modes.GROUND;
    this.states = {
      GND : 1,
      UNDER_50FT : 2,
      OVER_50FT : 3 
    }
    this.oldPlaneState = this.states.GND;
    this.currentPlaneState = this.states.GND;
    this.modeTransition = false;
    this.newStateTime = 0;
    this.update = function(dt) {
      this.detectState();
      this.detectMode(dt);
    }
    this.detectMode = function(dt) {
      const planeState = this.currentPlaneState;
      if(this.modeTransition && planeState == this.oldPlaneState) {
        this.newStateTime += dt;
      }
      else if(!this.modeTransition && planeState != this.oldPlaneState) {
        this.startTransition();
      }
      this.oldPlaneState = planeState;
      if(this.modeTransition) {
        if       (this.mode == this.modes.GROUND) {
          if(this.newStateTime > 5.0) {
            this.mode = this.modes.FLIGHT;
            this.finishTransition();
            xplog('switch to FLIGHT MODE: FLT > 5 sec');
          } else if(this.RA_OV_50() || (this.FLT() && this.PITCH_ATT_OV8())) {
            this.mode = this.modes.FLIGHT;
            this.finishTransition();
            xplog('switch to FLIGHT MODE: ' + 'RA: ' + this.RA_OV_50() + ', FLT: ' + this.FLT() + ', PITCH_ATT_OV 8 deg: ' + this.PITCH_ATT_OV8());
          }
        } else if(this.mode == this.modes.FLIGHT) {
          if(this.newStateTime > 1.0 && this.RA_OV_50() == false) {
            this.mode = this.modes.FLARE;
            this.finishTransition();
            xplog('switch to FLARE MODE: ' + 'RA: ' + !this.RA_OV_50());
          }
        } else if(this.mode == this.modes.FLARE) {
          if(this.GND_OV_5() && this.PITCH_ATT_LE_2_5()) {
            this.mode = this.modes.GROUND;
            this.finishTransition();
            xplog('switch to GROUND MODE: ' + 'GND 5sec: ' + this.GND_OV_5() + ', PITCH_ATT_LESS_2.5 deg: ' + this.PITCH_ATT_LE_2_5());
          }
          else if(this.newStateTime > 1.0 && this.RA_OV_50()) {
            this.mode = this.modes.FLIGHT;
            this.finishTransition();
            xplog('switch to FLIGHT MODE: ' + 'RA: ' + this.RA_OV_50());
          }
        }
      }
    }
    this.detectState = function() {
      if(get(gndRef) == 1) {
        this.currentPlaneState = this.states.GND;
      } else {
        if(get(RADataRef) < 50)
          this.currentPlaneState = this.states.UNDER_50FT;
        else
          this.currentPlaneState = this.states.OVER_50FT;
      }
    }
    this.startTransition = function() {
      this.newStateTime = 0;
      this.modeTransition = true;
      xplog('initiate mode transition');
    }
    this.finishTransition = function() {
      this.modeTransition = false;
      this.newStateTime = 0;
    }
    this.GND_OV_5 = function() {
      if(get(gndRef) == 1 && this.newStateTime > 5.0)
        return true;
      return false;
    }
    this.FLT = function() {
      if(get(gndRef) != 1)
        return true;
      return false;
    }
    this.PITCH_ATT_OV8 = function() {
      if(get(pitchDataRef) > 8)
        return true;
      return false;
    }
    this.PITCH_ATT_LE_2_5 = function() {
      if(get(pitchDataRef) < 2.5)
        return true;
      return false;
    }
    this.RA_OV_50 = function() {
      if(get(RADataRef) > 50)
        return true;
      return false;
    }

  }

  this.pInput = pctl;
  this.surfaces = surfaces;

  /*
   * this are 'high speed' coefficients, on low speeds controller behave slugish and not responsive
   * so we should modify coefficients or add 'low-speed' controllers
   */
  this.pidG = new PID(0.18, 4, 0.008, 0.01);
  this.pidPitch = new PID(0.025, 0.01, 0.018, 1);
  this.pidElevDef = new PID(0.5, 0, 0, 0);
  this.pidRoll = new PID(0.025, 0.01, 0.018, 0.1);

  this.dt = 0.0;

  this.setPitch = 0.0;
  this.setRoll = 0.0;

  this.wasNeutral = false;

  this.controlMode = new ControlMode();

  this.update = function(dt) {
    this.dt = dt;
    this.pInput.update();
    this.controlMode.update(dt);
    if(this.controlMode.mode == this.controlMode.modes.GROUND)
      this.direct();
    else
      this.normalLaw();
    this.surfaces.update(dt);
  }
  this.direct = function() {
    this.surfaces.setElevFrac(this.dt, -(this.pInput.pitchInput-0.5));
    this.surfaces.setAilrDefFrac(this.dt, this.pInput.rollInput-0.5);
  }
  this.flare = function() {
    this.surfaces.setElevFrac(this.dt, -(this.pInput.pitchInput-0.5));
    this.surfaces.setAilrDefFrac(this.dt, this.pInput.rollInput-0.5);
  }
  this.normalLaw = function() {
    if(this.pInput.isNeutral() && this.controlMode.mode != this.controlMode.modes.FLARE) {
      if(this.wasNeutral != true) {
        this.rememberAttitude();
        this.wasNeutral = true;
        xplog('input neutral, remember attitude: pitch: ' + this.setPitch + ' and roll: ' + this.setRoll);
      }
      this.holdAttitude();
    }
    else {
//      this.loadFactorDemand();
//      this.rollProt();
      // for now use direct law, side-stick in airbus controls G load in normal law
      this.direct();
      this.wasNeutral = false;
    }
  }
  /*
   * return load factor for side stick deflection according to airbus specs
   * on clean config load factor varies from -1g to +2.5g
   * otherweis from 0g to 2.5g
   * input varies from 0 to 1 with 0.5 as center
   */
  this.loadFactorFunction = function(input) {
    if(this.surfaces.isCleanConfig()) {
      // load Factor from +2.5 to -1
      if(input < 0.5) {
        return 4*input-1;
      } else {
        return 3*input-0.5;
      }
    } else {
      // load factor from +2 to 0
      return input*2;
    }
  }
  this.loadFactorDemand = function() {
    var messG = get(gNrmlDataRef);
    var messPitch = get(pitchDataRef);
    var setG = this.loadFactorFunction(this.pInput.pitchInput);
    xplog('setG: ' + setG + ', messG: ' + messG);
    var outPitch = this.pidG.update(this.dt, setG, messG);
//    var outDef = this.pidPitch.update(this.dt, outPitch, messPitch);
//    xplog(outDef + ', '  + messPitch + ', '  + outPitch + ', ' + messG);
    this.surfaces.setElevFrac(this.dt, -outPitch);
  }
  this.holdAttitude = function() {
    var messPitch = get(pitchDataRef);
    var messRoll = get(rollDataRef);
    var outDeflection = this.pidPitch.update(this.dt, this.setPitch, messPitch);
    var out = this.pidElevDef.update(this.dt, outDeflection, this.surfaces.elev.elevDeg);

    var outRoll = this.pidRoll.update(this.dt, this.setRoll, messRoll);

    this.surfaces.setElevFrac(this.dt, out);
    this.surfaces.setAilrDefFrac(this.dt, outRoll);
//    if(this.controlMode.mode != this.controlMode.modes.FLARE)
//      this.surfaces.adjustElevatorTrim(this.dt);
  }
  this.rollProt = function() {
    this.surfaces.setAilrDefFrac(this.dt, (this.pInput.rollInput-0.5));
  }
  this.rememberAttitude = function() {
    this.setPitch = get(pitchDataRef);
    this.setRoll = get(rollDataRef);
  }
  this.auto = function() {
  }
}

// Init flight controls, pilot input and fcc (fbw)
var pctl = new PilotCtrls(0, 1, 2);
var sa = new CtrlSurfacesAssignment(2, 1, 8, 9, 0);
var surfaces = new CtrlSurfaces(17.0, 30.0, 30.0, sa);

var fcc = new FCC(pctl, surfaces);


// callbacks for plugin
// onEnable is called from XPluginEnable (when plugin gets enabled), before loop callbacks are registred
function onEnable() {
  xplog('enabled, setting fctl override');
  set(overrideSurfaces, 1);
  set(overrideFlightControl, 1);
}

// callbacks for plugin
// onEnable is called from XPluginDisable (when plugin gets disabled), after loop callbacks are unregistred
function onDisable() {
  xplog('enabled, releasing fctl override');
  set(overrideSurfaces, 0);
  set(overrideFlightControl, 0);
}

function update() {

  curTime = get(timerDataRef);
  // update only 15 times per second
  if(curTime - prevTime > 0.066) {
    fcc.update(0.066);
    prevTime = curTime;
  }
}



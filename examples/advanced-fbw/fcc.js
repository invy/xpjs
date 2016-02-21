
const Controller = imports.pid;

const gndRef = requestDRef('sim/flightmodel/failures/onground_any');

const gNrmlDataRef = requestDRef('sim/flightmodel/forces/g_nrml');
const pitchDataRef = requestDRef('sim/cockpit2/gauges/indicators/pitch_electric_deg_pilot');
const rollDataRef = requestDRef('sim/cockpit2/gauges/indicators/roll_electric_deg_pilot');

const RADataRef = requestDRef('sim/cockpit2/gauges/indicators/radio_altimeter_height_ft_pilot');



/*
 * Flight Contorl Computer aka Fly-by-Wire
 *  Implements: Autotrim, holding attitude, flight envelope (todo)
 */
function FCC(pctl, surfaces) {
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
  this.pidG = new Controller.PID(0.18, 4, 0.008, 0.01);
  this.pidPitch = new Controller.PID(0.025, 0.01, 0.018, 1);
  this.pidElevDef = new Controller.PID(0.5, 0, 0, 0);
  this.pidRoll = new Controller.PID(0.025, 0.01, 0.018, 0.1);

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

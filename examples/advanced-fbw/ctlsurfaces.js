
const MH = imports.mathhelper;


const hstabElev = requestDRef('sim/flightmodel2/wing/elevator1_deg');
const ail1 = requestDRef('sim/flightmodel2/wing/aileron1_deg');

const elevatorTrim = requestDRef('sim/flightmodel2/controls/elevator_trim');
//const elevTrimDref = requestDRef('sim/flightmodel2/controls/stabilizer_deflection_degrees');
const elevTrimUpMaxDref = requestDRef('sim/aircraft/controls/acf_hstb_trim_up');
const elevTrimDnMaxDref = requestDRef('sim/aircraft/controls/acf_hstb_trim_dn');

const flapsHandleDeployRatio = requestDRef('sim/flightmodel2/controls/flap_handle_deploy_ratio');
const flapsDeployRatioDRef = requestDRef('sim/flightmodel2/controls/flap1_deploy_ratio');
const lWingFlapsAngle = requestDRef('sim/flightmodel/controls/wing1l_fla1def');
const rWingFlapsAngle = requestDRef('sim/flightmodel/controls/wing1r_fla1def');
const slatsRat = requestDRef('sim/flightmodel2/controls/slat1_deploy_ratio');

/*
 * Control surfaces assignment
 */
function CtrlSurfacesAssignment(aLeft, aRight, eLeft, eRight, r) {
  this.aileronLeft = aLeft;
  this.aileronRight = aRight;
  this.elevatorLeft = eLeft;
  this.elevatorRight = eRight;
  this.ruddder = r;
}

/*
 * Control surfaces
 */
function CtrlSurfaces(elevMax, ailrMax, ruddMax, a) {
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
      this.ailrDeg = MH.limitMinMax(deg, -this.ailrMax, this.ailrMax);
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
      this.ailrDeg = MH.limitMinMax(this.ailrDeg, -this.ailrMax, this.ailrMax);
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
      this.elevDeg = MH.limitMinMax(deg, -this.elevMax, this.elevMax);
    }
    this.setElevDeg = function(dt, deg) {
//    if(this.areEqual(deg, this.elevDeg))
//      return;
      if(this.elevDeg < deg)
        this.elevDeg += Math.min(this.elevDefRate * dt, deg - this.elevDeg);
      else if(this.elevDeg > deg)
        this.elevDeg -= Math.min(this.elevDefRate * dt, this.elevDeg - deg);
      this.elevDeg = MH.limitMinMax(this.elevDeg, -this.elevMax, this.elevMax);
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
      const slatPos = get(this.slatsRat);
      if(get(flapsHandleDeployRatio) > 0 && slatPos < 1) {
        var newPos = Math.min(slatPos+0.3*dt, 1);
        set(this.slatsRat, newPos);
      }
      else if(get(flapsHandleDeployRatio) == 0 && slatPos > 0) {
        var newPos = Math.max(slatPos-0.3*dt, 0);
        set(this.slatsRat, newPos);
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


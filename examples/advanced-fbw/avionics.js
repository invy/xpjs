
const Input = imports.pilotctrls;
const Surfaces = imports.ctlsurfaces;
const FCC = imports.fcc;


xplog('plugin started at: ' +new Date());

// data ref definition
const timerDataRef = requestDRef('sim/time/total_running_time_sec');
const overrideSurfaces = requestDRef('sim/operation/override/override_control_surfaces');
const overrideFlightControl = requestDRef('sim/operation/override/override_flightcontrol');

// init script values
var prevTime = get(timerDataRef);
var curTime = 0;


// Init control surfaces, pilot input and fcc (fbw)
var pctl = new Input.PilotCtrls(0, 1, 2);
var sa = new Surfaces.CtrlSurfacesAssignment(2, 1, 8, 9, 0);
var surfaces = new Surfaces.CtrlSurfaces(17.0, 30.0, 30.0, sa);

var fcc = new FCC.FCC(pctl, surfaces);


// callbacks for plugin
// onEnable is called from XPluginEnable (when plugin gets enabled), before loop callbacks are registred
onEnable = function() {
  xplog('enabled, setting fctl override');
  set(overrideSurfaces, 1);
  set(overrideFlightControl, 1);
}

// callbacks for plugin
// onEnable is called from XPluginDisable (when plugin gets disabled), after loop callbacks are unregistred
onDisable = function() {
  xplog('enabled, releasing fctl override');
  set(overrideSurfaces, 0);
  set(overrideFlightControl, 0);
}

update = function() {
  curTime = get(timerDataRef);
  // update only 15 times per second
  if(curTime - prevTime > 0.066) {
    fcc.update(0.066);
    prevTime = curTime;
  }
}


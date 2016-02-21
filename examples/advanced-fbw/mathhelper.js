
function limitAbs(v, l) {
  if(v > l)
    return l;
  if(-v < -l)
    return -l;
  return v;
}

function limit(v, l) {
  if(v > l)
    return l;
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

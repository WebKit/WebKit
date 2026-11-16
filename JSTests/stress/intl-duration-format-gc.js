(function()
{
  gc()})()
function __f_22(__v_64) {
  return __v_64.hasOwnProperty("prototype") &&
         __v_64.prototype.hasOwnProperty("resolvedOptions")}
function __f_25(__v_67) {
  __v_68 = []
  try {
    if (__v_67 ==Intl.DisplayNames)
      try {
        __v_68 = [ , {type : "language"} ]} catch {}
  } catch {}
  new __v_67(...__v_68)  }
    Object.getOwnPropertyNames(Intl)
                     .map(__v_71 => Intl[__v_71])
                     .filter(__f_22).filter( __f_25)

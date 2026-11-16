//@ requireOptions("--jitPolicyScale=0", "--useConcurrentJIT=0")

function opt() {
  function f(){
  }((Error)[flag?0:(WebAssembly)])++
}
flag=true
try{opt()}catch{}
flag=false
try{opt()}catch{}
flag=true
try{opt()}catch{}

(function(Object_a_){typeof
globalThis!=="object"&&(this?get_b_():(Object_a_.defineProperty(Object_a_.prototype,"_T_",{configurable:true,get:get_b_}),_T_));function
get_b_(){var
global_b_=this||self;global_b_.globalThis=global_b_;delete
Object_a_.prototype._T_}}(Object));(js___=>async args_a_=>{"use strict";const{link:link_j_,src:src_X_,generated:generated_L_}=args_a_,isNode_e_=globalThis.process?.versions?.node,isShell_g_=!globalThis.TextDecoder,math_U_={cos:Math.cos,sin:Math.sin,tan:Math.tan,acos:Math.acos,asin:Math.asin,atan:Math.atan,cosh:Math.cosh,sinh:Math.sinh,tanh:Math.tanh,acosh:Math.acosh,asinh:Math.asinh,atanh:Math.atanh,cbrt:Math.cbrt,exp:Math.exp,expm1:Math.expm1,log:Math.log,log1p:Math.log1p,log2:Math.log2,log10:Math.log10,atan2:Math.atan2,hypot:Math.hypot,pow:Math.pow,fmod:(x_a_,y_b_)=>x_a_%y_b_},typed_arrays_z_=[Float32Array,Float64Array,Int8Array,Uint8Array,Int16Array,Uint16Array,Int32Array,Int32Array,Int32Array,Int32Array,Float32Array,Float64Array,Uint8Array,Uint16Array,Uint8ClampedArray],fs_f_=isNode_e_&&require("node:fs"),fs_cst_b_=fs_f_?.constants,access_flags_A_=fs_f_?[fs_cst_b_.R_OK,fs_cst_b_.W_OK,fs_cst_b_.X_OK,fs_cst_b_.F_OK]:[],open_flags_V_=fs_f_?[fs_cst_b_.O_RDONLY,fs_cst_b_.O_WRONLY,fs_cst_b_.O_RDWR,fs_cst_b_.O_APPEND,fs_cst_b_.O_CREAT,fs_cst_b_.O_TRUNC,fs_cst_b_.O_EXCL,fs_cst_b_.O_NONBLOCK,fs_cst_b_.O_NOCTTY,fs_cst_b_.O_DSYNC,fs_cst_b_.O_SYNC]:[];var
out_channels_c_={map:new
WeakMap(),set:new
Set(),finalization:new
FinalizationRegistry(ref_a_=>out_channels_c_.set.delete(ref_a_))};function
register_channel_W_(ch_a_){const
ref_b_=new
WeakRef(ch_a_);out_channels_c_.map.set(ch_a_,ref_b_);out_channels_c_.set.add(ref_b_);out_channels_c_.finalization.register(ch_a_,ref_b_,ch_a_)}function
unregister_channel_Y_(ch_a_){const
ref_b_=out_channels_c_.map.get(ch_a_);if(ref_b_){out_channels_c_.map.delete(ch_a_);out_channels_c_.set.delete(ref_b_);out_channels_c_.finalization.unregister(ch_a_)}}function
channel_list_I_(){return[...out_channels_c_.set].map(ref_a_=>ref_a_.deref()).filter(ch_a_=>ch_a_)}var
start_fiber_y_;function
make_suspending_S_(f_a_){return WebAssembly?.Suspending?new
WebAssembly.Suspending(f_a_):f_a_}function
make_promising_u_(f_a_){return WebAssembly?.promising&&f_a_?WebAssembly.promising(f_a_):f_a_}const
decoder_n_=isShell_g_||new
TextDecoder("utf-8",{ignoreBOM:1}),encoder_J_=isShell_g_||new
TextEncoder();function
hash_int_M_(h_a_,d_b_){d_b_=Math.imul(d_b_,0xcc9e2d51|0);d_b_=d_b_<<15|d_b_>>>17;d_b_=Math.imul(d_b_,0x1b873593);h_a_^=d_b_;h_a_=h_a_<<13|h_a_>>>19;return(h_a_+(h_a_<<2)|0)+(0xe6546b64|0)|0}function
hash_string_N_(h_a_,s_b_){for(var
i_c_=0;i_c_<s_b_.length;i_c_++)h_a_=hash_int_M_(h_a_,s_b_.charCodeAt(i_c_));return h_a_^s_b_.length}function
getenv_t_(n_a_){if(isNode_e_&&globalThis.process.env[n_a_]!==undefined)return globalThis.process.env[n_a_];return globalThis.jsoo_env?.[n_a_]}let
record_backtrace_flag_k_=0;for(const
l_a_
of
getenv_t_("OCAMLRUNPARAM")?.split(",")||[]){if(l_a_==="b")record_backtrace_flag_k_=1;if(l_a_.startsWith("b="))record_backtrace_flag_k_=+l_a_.slice(2)?1:0}function
alloc_stat_m_(s_a_,large_b_){var
kind_c_;if(s_a_.isFile())kind_c_=0;else if(s_a_.isDirectory())kind_c_=1;else if(s_a_.isCharacterDevice())kind_c_=2;else if(s_a_.isBlockDevice())kind_c_=3;else if(s_a_.isSymbolicLink())kind_c_=4;else if(s_a_.isFIFO())kind_c_=5;else if(s_a_.isSocket())kind_c_=6;return caml_alloc_stat_E_(large_b_,s_a_.dev,s_a_.ino|0,kind_c_,s_a_.mode,s_a_.nlink,s_a_.uid,s_a_.gid,s_a_.rdev,BigInt(s_a_.size),s_a_.atimeMs/1000,s_a_.mtimeMs/1000,s_a_.ctimeMs/1000)}const
on_windows_v_=isNode_e_&&globalThis.process.platform==="win32",bindings_B_={jstag:WebAssembly.JSTag||new
WebAssembly.Tag({parameters:["externref"],results:[]}),identity:x_a_=>x_a_,from_bool:x_a_=>!!x_a_,get:(x_a_,y_b_)=>x_a_[y_b_],set:(x_a_,y_b_,z_c_)=>x_a_[y_b_]=z_c_,delete:(x_a_,y_b_)=>delete
x_a_[y_b_],instanceof:(x_a_,y_b_)=>x_a_
instanceof
y_b_,typeof:x_a_=>typeof
x_a_,equals:(x_a_,y_b_)=>x_a_==y_b_,strict_equals:(x_a_,y_b_)=>x_a_===y_b_,fun_call:(f_a_,o_b_,args_c_)=>f_a_.apply(o_b_,args_c_),meth_call:(o_a_,f_b_,args_c_)=>o_a_[f_b_].apply(o_a_,args_c_),new_array:n_a_=>new
Array(n_a_),new_obj:()=>({}),new:(c_a_,args_b_)=>new
c_a_(...args_b_),global_this:globalThis,iter_props:(o_a_,f_b_)=>{for(var
nm_c_
in
o_a_)if(Object.hasOwn(o_a_,nm_c_))f_b_(nm_c_)},array_length:a_a_=>a_a_.length,array_get:(a_a_,i_b_)=>a_a_[i_b_],array_set:(a_a_,i_b_,v_c_)=>a_a_[i_b_]=v_c_,read_string:l_a_=>isShell_g_?decodeURIComponent(escape(String.fromCharCode(...new
Uint8Array(buffer_i_,0,l_a_)))):decoder_n_.decode(new
Uint8Array(buffer_i_,0,l_a_)),read_string_stream:(l_a_,stream_b_)=>decoder_n_.decode(new
Uint8Array(buffer_i_,0,l_a_),{stream:stream_b_}),append_string:(s1_a_,s2_b_)=>s1_a_+s2_b_,write_string:s_a_=>{if(isShell_g_){s_a_=unescape(encodeURIComponent(s_a_));for(let
i_b_=0;i_b_<s_a_.length;++i_b_)out_buffer_x_[i_b_]=s_a_.charCodeAt(i_b_);return s_a_.length}var
start_c_=0,len_b_=s_a_.length;for(;;){const{read:read_d_,written:written_e_}=encoder_J_.encodeInto(s_a_.slice(start_c_),out_buffer_x_);len_b_-=read_d_;if(!len_b_)return written_e_;caml_extract_bytes_G_(written_e_);start_c_+=read_d_}},ta_create:(k_a_,sz_b_)=>new
typed_arrays_z_[k_a_](sz_b_),ta_normalize:a_a_=>a_a_
instanceof
Uint32Array?new
Int32Array(a_a_.buffer,a_a_.byteOffset,a_a_.length):a_a_,ta_kind:a_b_=>typed_arrays_z_.findIndex(c_a_=>a_b_
instanceof
c_a_),ta_length:a_a_=>a_a_.length,ta_get_f64:(a_a_,i_b_)=>a_a_[i_b_],ta_get_f32:(a_a_,i_b_)=>a_a_[i_b_],ta_get_i32:(a_a_,i_b_)=>a_a_[i_b_],ta_get_i16:(a_a_,i_b_)=>a_a_[i_b_],ta_get_ui16:(a_a_,i_b_)=>a_a_[i_b_],ta_get_i8:(a_a_,i_b_)=>a_a_[i_b_],ta_get_ui8:(a_a_,i_b_)=>a_a_[i_b_],ta_get16_ui8:(a_a_,i_b_)=>a_a_[i_b_]|a_a_[i_b_+1]<<8,ta_get32_ui8:(a_a_,i_b_)=>a_a_[i_b_]|a_a_[i_b_+1]<<8|a_a_[i_b_+2]<<16|a_a_[i_b_+3]<<24,ta_set_f64:(a_a_,i_b_,v_c_)=>a_a_[i_b_]=v_c_,ta_set_f32:(a_a_,i_b_,v_c_)=>a_a_[i_b_]=v_c_,ta_set_i32:(a_a_,i_b_,v_c_)=>a_a_[i_b_]=v_c_,ta_set_i16:(a_a_,i_b_,v_c_)=>a_a_[i_b_]=v_c_,ta_set_ui16:(a_a_,i_b_,v_c_)=>a_a_[i_b_]=v_c_,ta_set_i8:(a_a_,i_b_,v_c_)=>a_a_[i_b_]=v_c_,ta_set_ui8:(a_a_,i_b_,v_c_)=>a_a_[i_b_]=v_c_,ta_set16_ui8:(a_a_,i_b_,v_c_)=>{a_a_[i_b_]=v_c_;a_a_[i_b_+1]=v_c_>>8},ta_set32_ui8:(a_a_,i_b_,v_c_)=>{a_a_[i_b_]=v_c_;a_a_[i_b_+1]=v_c_>>8;a_a_[i_b_+2]=v_c_>>16;a_a_[i_b_+3]=v_c_>>24},ta_fill:(a_a_,v_b_)=>a_a_.fill(v_b_),ta_blit:(s_a_,d_b_)=>d_b_.set(s_a_),ta_subarray:(a_a_,i_b_,j_c_)=>a_a_.subarray(i_b_,j_c_),ta_set:(a_a_,b_b_,i_c_)=>a_a_.set(b_b_,i_c_),ta_new:len_a_=>new
Uint8Array(len_a_),ta_copy:(ta_a_,t_b_,s_c_,e_d_)=>ta_a_.copyWithin(t_b_,s_c_,e_d_),ta_bytes:a_a_=>new
Uint8Array(a_a_.buffer,a_a_.byteOffset,a_a_.length*a_a_.BYTES_PER_ELEMENT),ta_blit_from_bytes:(s_a_,p1_b_,a_c_,p2_d_,l_e_)=>{for(let
i_f_=0;i_f_<l_e_;i_f_++)a_c_[p2_d_+i_f_]=bytes_get_C_(s_a_,p1_b_+i_f_)},ta_blit_to_bytes:(a_a_,p1_b_,s_c_,p2_d_,l_e_)=>{for(let
i_f_=0;i_f_<l_e_;i_f_++)bytes_set_D_(s_c_,p2_d_+i_f_,a_a_[p1_b_+i_f_])},wrap_callback:f_b_=>function(...args_a_){if(args_a_.length===0)args_a_=[undefined];return caml_callback_d_(f_b_,args_a_.length,args_a_,1)},wrap_callback_args:f_b_=>function(...args_a_){return caml_callback_d_(f_b_,1,[args_a_],0)},wrap_callback_strict:(arity_c_,f_b_)=>function(...args_a_){args_a_.length=arity_c_;return caml_callback_d_(f_b_,arity_c_,args_a_,0)},wrap_callback_unsafe:f_b_=>function(...args_a_){return caml_callback_d_(f_b_,args_a_.length,args_a_,2)},wrap_meth_callback:f_b_=>function(...args_a_){args_a_.unshift(this);return caml_callback_d_(f_b_,args_a_.length,args_a_,1)},wrap_meth_callback_args:f_b_=>function(...args_a_){return caml_callback_d_(f_b_,2,[this,args_a_],0)},wrap_meth_callback_strict:(arity_c_,f_b_)=>function(...args_a_){args_a_.length=arity_c_;args_a_.unshift(this);return caml_callback_d_(f_b_,args_a_.length,args_a_,0)},wrap_meth_callback_unsafe:f_b_=>function(...args_a_){args_a_.unshift(this);return caml_callback_d_(f_b_,args_a_.length,args_a_,2)},wrap_fun_arguments:f_b_=>function(...args_a_){return f_b_(args_a_)},format_float:(prec_a_,conversion_b_,pad_c_,x_d_)=>{function
toFixed_j_(x_a_,dp_b_){if(Math.abs(x_a_)<1.0)return x_a_.toFixed(dp_b_);else{var
e_c_=Number.parseInt(x_a_.toString().split("+")[1]);if(e_c_>20){e_c_-=20;x_a_/=Math.pow(10,e_c_);x_a_+=new
Array(e_c_+1).join("0");if(dp_b_>0)x_a_=x_a_+"."+new
Array(dp_b_+1).join("0");return x_a_}else
return x_a_.toFixed(dp_b_)}}switch(conversion_b_){case
0:var
s_e_=x_d_.toExponential(prec_a_),i_f_=s_e_.length;if(s_e_.charAt(i_f_-3)==="e")s_e_=s_e_.slice(0,i_f_-1)+"0"+s_e_.slice(i_f_-1);break;case
1:s_e_=toFixed_j_(x_d_,prec_a_);break;case
2:prec_a_=prec_a_?prec_a_:1;s_e_=x_d_.toExponential(prec_a_-1);var
j_i_=s_e_.indexOf("e"),exp_h_=+s_e_.slice(j_i_+1);if(exp_h_<-4||x_d_>=1e21||x_d_.toFixed(0).length>prec_a_){var
i_f_=j_i_-1;while(s_e_.charAt(i_f_)==="0")i_f_--;if(s_e_.charAt(i_f_)===".")i_f_--;s_e_=s_e_.slice(0,i_f_+1)+s_e_.slice(j_i_);i_f_=s_e_.length;if(s_e_.charAt(i_f_-3)==="e")s_e_=s_e_.slice(0,i_f_-1)+"0"+s_e_.slice(i_f_-1);break}else{var
p_g_=prec_a_;if(exp_h_<0){p_g_-=exp_h_+1;s_e_=x_d_.toFixed(p_g_)}else
while(s_e_=x_d_.toFixed(p_g_),s_e_.length>prec_a_+1)p_g_--;if(p_g_){var
i_f_=s_e_.length-1;while(s_e_.charAt(i_f_)==="0")i_f_--;if(s_e_.charAt(i_f_)===".")i_f_--;s_e_=s_e_.slice(0,i_f_+1)}}break}return pad_c_?" "+s_e_:s_e_},gettimeofday:()=>new
Date().getTime()/1000,times:()=>{if(globalThis.process?.cpuUsage){var
t_a_=globalThis.process.cpuUsage();return caml_alloc_times_p_(t_a_.user/1e6,t_a_.system/1e6)}else{var
t_a_=performance.now()/1000;return caml_alloc_times_p_(t_a_,t_a_)}},gmtime:t_a_=>{var
d_b_=new
Date(t_a_*1000),d_num_c_=d_b_.getTime(),januaryfirst_e_=new
Date(Date.UTC(d_b_.getUTCFullYear(),0,1)).getTime(),doy_d_=Math.floor((d_num_c_-januaryfirst_e_)/86400000);return caml_alloc_tm_q_(d_b_.getUTCSeconds(),d_b_.getUTCMinutes(),d_b_.getUTCHours(),d_b_.getUTCDate(),d_b_.getUTCMonth(),d_b_.getUTCFullYear()-1900,d_b_.getUTCDay(),doy_d_,false)},localtime:t_a_=>{var
d_b_=new
Date(t_a_*1000),d_num_c_=d_b_.getTime(),januaryfirst_f_=new
Date(d_b_.getFullYear(),0,1).getTime(),doy_d_=Math.floor((d_num_c_-januaryfirst_f_)/86400000),jan_e_=new
Date(d_b_.getFullYear(),0,1),jul_g_=new
Date(d_b_.getFullYear(),6,1),stdTimezoneOffset_h_=Math.max(jan_e_.getTimezoneOffset(),jul_g_.getTimezoneOffset());return caml_alloc_tm_q_(d_b_.getSeconds(),d_b_.getMinutes(),d_b_.getHours(),d_b_.getDate(),d_b_.getMonth(),d_b_.getFullYear()-1900,d_b_.getDay(),doy_d_,d_b_.getTimezoneOffset()<stdTimezoneOffset_h_)},mktime:(year_a_,month_b_,day_c_,h_d_,m_e_,s_f_)=>new
Date(year_a_,month_b_,day_c_,h_d_,m_e_,s_f_).getTime(),random_seed:()=>crypto.getRandomValues(new
Int32Array(12)),access:(p_a_,flags_d_)=>fs_f_.accessSync(p_a_,access_flags_A_.reduce((f_a_,v_b_,i_c_)=>flags_d_&1<<i_c_?f_a_|v_b_:f_a_,0)),open:(p_a_,flags_d_,perm_c_)=>fs_f_.openSync(p_a_,open_flags_V_.reduce((f_a_,v_b_,i_c_)=>flags_d_&1<<i_c_?f_a_|v_b_:f_a_,0),perm_c_),close:fd_a_=>fs_f_.closeSync(fd_a_),write:(fd_a_,b_b_,o_c_,l_d_,p_e_)=>fs_f_?fs_f_.writeSync(fd_a_,b_b_,o_c_,l_d_,p_e_===null?p_e_:Number(p_e_)):((isShell_g_?function() { }:console[fd_a_===2?"error":"log"])(typeof
b_b_==="string"?b_b_:isShell_g_?decodeURIComponent(escape(String.fromCharCode(...b_b_.slice(o_c_,o_c_+l_d_)))):decoder_n_.decode(b_b_.slice(o_c_,o_c_+l_d_))),l_d_),read:(fd_a_,b_b_,o_c_,l_d_,p_e_)=>fs_f_.readSync(fd_a_,b_b_,o_c_,l_d_,p_e_),fsync:fd_a_=>fs_f_.fsyncSync(fd_a_),file_size:fd_a_=>fs_f_.fstatSync(fd_a_,{bigint:true}).size,register_channel:register_channel_W_,unregister_channel:unregister_channel_Y_,channel_list:channel_list_I_,exit:n_a_=>isNode_e_&&globalThis.process.exit(n_a_),argv:()=>isNode_e_?globalThis.process.argv.slice(1):["a.out"],on_windows:+on_windows_v_,getenv:getenv_t_,backtrace_status:()=>record_backtrace_flag_k_,record_backtrace:b_a_=>record_backtrace_flag_k_=b_a_,system:c_a_=>{var
res_b_=require("node:child_process").spawnSync(c_a_,{shell:true,stdio:"inherit"});if(res_b_.error)throw res_b_.error;return res_b_.signal?255:res_b_.status},isatty:fd_a_=>+require("node:tty").isatty(fd_a_),time:()=>performance.now(),getcwd:()=>isNode_e_?globalThis.process.cwd():"/static",chdir:x_a_=>globalThis.process.chdir(x_a_),mkdir:(p_a_,m_b_)=>fs_f_.mkdirSync(p_a_,m_b_),rmdir:p_a_=>fs_f_.rmdirSync(p_a_),link:(d_a_,s_b_)=>fs_f_.linkSync(d_a_,s_b_),symlink:(t_a_,p_b_,kind_c_)=>fs_f_.symlinkSync(t_a_,p_b_,[null,"file","dir"][kind_c_]),readlink:p_a_=>fs_f_.readlinkSync(p_a_),unlink:p_a_=>fs_f_.unlinkSync(p_a_),read_dir:p_a_=>fs_f_.readdirSync(p_a_),opendir:p_a_=>fs_f_.opendirSync(p_a_),readdir:d_a_=>{var
n_b_=d_a_.readSync()?.name;return n_b_===undefined?null:n_b_},closedir:d_a_=>d_a_.closeSync(),stat:(p_a_,l_b_)=>alloc_stat_m_(fs_f_.statSync(p_a_),l_b_),lstat:(p_a_,l_b_)=>alloc_stat_m_(fs_f_.lstatSync(p_a_),l_b_),fstat:(fd_a_,l_b_)=>alloc_stat_m_(fs_f_.fstatSync(fd_a_),l_b_),chmod:(p_a_,perms_b_)=>fs_f_.chmodSync(p_a_,perms_b_),fchmod:(p_a_,perms_b_)=>fs_f_.fchmodSync(p_a_,perms_b_),file_exists:p_a_=>isShell_g_?0:+fs_f_.existsSync(p_a_),is_directory:p_a_=>+fs_f_.lstatSync(p_a_).isDirectory(),is_file:p_a_=>+fs_f_.lstatSync(p_a_).isFile(),utimes:(p_a_,a_b_,m_c_)=>fs_f_.utimesSync(p_a_,a_b_,m_c_),truncate:(p_a_,l_b_)=>fs_f_.truncateSync(p_a_,l_b_),ftruncate:(fd_a_,l_b_)=>fs_f_.ftruncateSync(fd_a_,l_b_),rename:(o_a_,n_b_)=>{var
n_stat_c_;if(on_windows_v_&&(n_stat_c_=fs_f_.statSync(n_b_,{throwIfNoEntry:false}))&&fs_f_.statSync(o_a_,{throwIfNoEntry:false})?.isDirectory())if(n_stat_c_.isDirectory()){if(!n_b_.startsWith(o_a_))try{fs_f_.rmdirSync(n_b_)}catch{}}else{var
e_d_=new
Error(`ENOTDIR: not a directory, rename '${o_a_}' -> '${n_b_}'`);throw Object.assign(e_d_,{errno:-20,code:"ENOTDIR",syscall:"rename",path:n_b_})}fs_f_.renameSync(o_a_,n_b_)},start_fiber:x_a_=>start_fiber_y_(x_a_),suspend_fiber:make_suspending_S_((f_c_,env_b_)=>new
Promise(k_a_=>f_c_(k_a_,env_b_))),resume_fiber:(k_a_,v_b_)=>k_a_(v_b_),weak_new:v_a_=>new
WeakRef(v_a_),weak_deref:w_a_=>{var
v_b_=w_a_.deref();return v_b_===undefined?null:v_b_},weak_map_new:()=>new
WeakMap(),map_new:()=>new
Map(),map_get:(m_a_,x_b_)=>{var
v_c_=m_a_.get(x_b_);return v_c_===undefined?null:v_c_},map_set:(m_a_,x_b_,v_c_)=>m_a_.set(x_b_,v_c_),map_delete:(m_a_,x_b_)=>m_a_.delete(x_b_),log:x_a_=>console.log(x_a_)},string_ops_o_={test:v_a_=>+(typeof
v_a_==="string"),compare:(s1_a_,s2_b_)=>s1_a_<s2_b_?-1:+(s1_a_>s2_b_),hash:hash_string_N_,decodeStringFromUTF8Array:()=>"",encodeStringToUTF8Array:()=>0,fromCharCodeArray:()=>""},imports_h_=Object.assign({Math:math_U_,bindings:bindings_B_,js:js___,"wasm:js-string":string_ops_o_,"wasm:text-decoder":string_ops_o_,"wasm:text-encoder":string_ops_o_,env:{}},generated_L_),options_w_={builtins:[]};function
loadRelative_R_(src_a_){const
path_b_=require("node:path"),f_c_=path_b_.join(path_b_.dirname(require.main.filename),src_a_);return require("node:fs/promises").readFile(f_c_)}const
fetchBase_s_=globalThis?.document?.currentScript?.src;function
fetchRelative_K_(src_a_){const
url_b_=fetchBase_s_?new
URL(src_a_,fetchBase_s_):src_a_;return fetch(url_b_)}const
loadCode_Q_=isNode_e_?loadRelative_R_:isShell_g_?s_a_=>globalThis.read(s_a_,"binary"):fetchRelative_K_;async function
instantiateModule_P_(code_a_){return isNode_e_||isShell_g_?WebAssembly.instantiate(await
code_a_,imports_h_,options_w_):WebAssembly.instantiateStreaming(code_a_,imports_h_,options_w_)}async function
instantiateFromDir_O_(){imports_h_.OCaml={};const
deps_c_=[];async function
loadModule_b_(module_a_,isRuntime_b_){const
sync_f_=module_a_[1].constructor!==Array;async function
instantiate_e_(){const
code_d_=loadCode_Q_(src_X_+"/"+module_a_[0]+".wasm");await
Promise.all(sync_f_?deps_c_:module_a_[1].map(i_a_=>deps_c_[i_a_]));const
wasmModule_e_=await
instantiateModule_P_(code_d_);Object.assign(isRuntime_b_?imports_h_.env:imports_h_.OCaml,wasmModule_e_.instance.exports)}const
promise_d_=instantiate_e_();deps_c_.push(promise_d_);return promise_d_}async function
loadModules_a_(lst_a_){for(const
module_c_
of
lst_a_)await
loadModule_b_(module_c_)}await
loadModule_b_(link_j_[0],1);if(link_j_.length>1){await
loadModule_b_(link_j_[1]);const
workers_c_=new
Array(20).fill(link_j_.slice(2).values()).map(loadModules_a_);await
Promise.all(workers_c_)}return{instance:{exports:Object.assign(imports_h_.env,imports_h_.OCaml)}}}const
wasmModule_Z_=await
instantiateFromDir_O_();var{caml_callback:caml_callback_d_,caml_alloc_times:caml_alloc_times_p_,caml_alloc_tm:caml_alloc_tm_q_,caml_alloc_stat:caml_alloc_stat_E_,caml_start_fiber:caml_start_fiber_H_,caml_handle_uncaught_exception:caml_handle_uncaught_exception_r_,caml_buffer:caml_buffer_F_,caml_extract_bytes:caml_extract_bytes_G_,bytes_get:bytes_get_C_,bytes_set:bytes_set_D_,_initialize:initialize_l_}=wasmModule_Z_.instance.exports,buffer_i_=caml_buffer_F_?.buffer,out_buffer_x_=buffer_i_&&new
Uint8Array(buffer_i_,0,buffer_i_.length);start_fiber_y_=make_promising_u_(caml_start_fiber_H_);var
initialize_l_=make_promising_u_(initialize_l_);if(globalThis.process?.on)globalThis.process.on("uncaughtException",(err_a_,origin_b_)=>caml_handle_uncaught_exception_r_(err_a_));else if(globalThis.addEventListener)globalThis.addEventListener("error",event_a_=>event_a_.error&&caml_handle_uncaught_exception_r_(event_a_.error));await
initialize_l_()})(function(globalThis_a_){"use strict";return{}}(globalThis))({"link":[["eqref-i31ref",0]],"generated":{},"src":"resources"});

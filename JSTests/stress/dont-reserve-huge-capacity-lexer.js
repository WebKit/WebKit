//@ if ($architecture != "x86-64") or $memoryLimited then skip else runDefault end

var fe="f";                                                                         
try
{
  for (i=0; i<25; i++)                                                   
    fe += fe;                                                            
                                                                         
  var fu=new Function(                                                   
    fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe,
    fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe,
    fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe, fe,
    fe, fe, fe, fe, fe, fe, fe, fe, fe, fe,                              
    "done"
    );
} catch(e) {
}

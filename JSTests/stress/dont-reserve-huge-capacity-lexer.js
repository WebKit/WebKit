//@ skip if ($architecture != "arm64" and $architecture != "x86-64")

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

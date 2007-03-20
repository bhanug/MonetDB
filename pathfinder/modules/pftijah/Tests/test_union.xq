let $opt := <TijahOptions 
                collection="thesis" 
                txtmodel_model="LMS" 
                txtmodel_returnall="false"
                debug="0"/>

let $query := "//(chapter|section)//para[about(.,information retrieval)]"

for $n at $r in tijah:query($opt,(),$query)
return <node rank="{$r}">{$n}</node>

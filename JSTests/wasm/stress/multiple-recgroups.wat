(module
 (rec
  (type $0 (sub (struct (field (ref null $1)))))
  (type $1 (sub $0 (struct (field (ref null $1)))))
 )
 (type $2 (func (result (ref $1))))
 (rec
  (type $3 (sub (struct (field i64))))
  (type $4 (sub $3 (struct (field i64))))
 )
 (export "test" (func $0))
 (func $0 (result (ref $1))
  (drop (struct.new $4 (i64.const 1)))
  (return (struct.new $1 (ref.null none)))
 )
)
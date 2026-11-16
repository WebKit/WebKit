
(module
    (type $AnyArray___type_0 (array (ref null any)))
    (type $FuncArray___type_1 (array (ref null func)))
    (type $SpecialITable___type_2 (struct (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field anyref) (field (ref null $FuncArray___type_1))))
    (type $LongArray___type_3 (array i64))
    (type $RTTI___type_4 (struct (field (ref null $LongArray___type_3)) (field (ref null $RTTI___type_4)) (field i32) (field i32) (field i32) (field i32) (field i32) (field i32) (field i64)))
    (type $<classVTable>___type_5 (struct (field (ref null $SpecialITable___type_2))))
    (type $kotlin.Any___type_6 (struct (field (ref $<classVTable>___type_5)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32))))
    (type $<classVTable>___type_7 (sub $<classVTable>___type_5 (struct (field (ref null $SpecialITable___type_2)))))
    (type $kotlin.Number___type_8 (sub $kotlin.Any___type_6 (struct (field (ref $<classVTable>___type_7)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)))))
    (type $<classVTable>___type_9 (sub $<classVTable>___type_5 (struct (field (ref null $SpecialITable___type_2)))))
    (type $kotlin.Companion___type_10 (sub $kotlin.Any___type_6 (struct (field (ref $<classVTable>___type_9)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut i32)) (field (mut i32)) (field (mut i32)) (field (mut i32)))))
    (type $<classVTable>___type_11 (sub $<classVTable>___type_5 (struct (field (ref null $SpecialITable___type_2)))))
    (type $kotlin.UInt___type_12 (sub $kotlin.Any___type_6 (struct (field (ref $<classVTable>___type_11)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut i32)))))
    (type $<classVTable>___type_13 (sub $<classVTable>___type_5 (struct (field (ref null $SpecialITable___type_2)))))
    (type $kotlin.wasm.internal.WasmAnyArray___type_14 (array (mut (ref null $kotlin.Any___type_6))))
    (type $kotlin.Array___type_15 (sub $kotlin.Any___type_6 (struct (field (ref $<classVTable>___type_13)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut (ref null $kotlin.wasm.internal.WasmAnyArray___type_14))))))
    (type $<classVTable>___type_16 (sub $<classVTable>___type_5 (struct (field (ref null $SpecialITable___type_2)))))
    (rec
        (type $kotlin.Companion___type_17 (sub $kotlin.Any___type_6 (struct (field (ref $<classVTable>___type_16)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut i32)) (field (mut i32)) (field (mut i32)) (field (mut i32)))))
        (type $mixin_1___type_18 (struct (field i31ref) (field i64))))
    (type $<classVTable>___type_19 (sub $<classVTable>___type_7 (struct (field (ref null $SpecialITable___type_2)))))
    (type $kotlin.Int___type_20 (sub $kotlin.Number___type_8 (struct (field (ref $<classVTable>___type_19)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut i32)))))
    (type $<classVTable>___type_21 (sub $<classVTable>___type_5 (struct (field (ref null $SpecialITable___type_2)))))
    (rec
        (type $kotlin.Companion___type_22 (sub $kotlin.Any___type_6 (struct (field (ref $<classVTable>___type_21)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)))))
        (type $mixin_1___type_23 (struct (field i31ref) (field i64))))
    (type $____type_24 (func (param (ref null $kotlin.Any___type_6)) (result i32)))
    (type $<classVTable>___type_25 (sub $<classVTable>___type_5 (struct (field (ref null $SpecialITable___type_2)) (field (ref null $____type_24)))))
    (type $kotlin.wasm.internal.WasmCharArray___type_26 (array (mut i16)))
    (type $kotlin.String___type_27 (sub $kotlin.Any___type_6 (struct (field (ref $<classVTable>___type_25)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut (ref null $kotlin.String___type_27))) (field (mut i32)) (field (mut (ref null $kotlin.wasm.internal.WasmCharArray___type_26))))))
    (type $kotlin.wasm.internal.WasmLongImmutableArray___type_28 (array i64))
    (type $____type_29 (func (param (ref null $kotlin.Any___type_6)) (result (ref null $kotlin.String___type_27))))
    (type $____type_30 (func (param (ref null $kotlin.Any___type_6)) (result externref)))
    (type $<classVTable>___type_31 (sub $<classVTable>___type_5 (struct (field (ref null $SpecialITable___type_2)) (field (ref null $____type_29)) (field (ref null $____type_30)))))
    (type $kotlin.Throwable___type_32 (sub $kotlin.Any___type_6 (struct (field (ref $<classVTable>___type_31)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Throwable___type_32))) (field (mut externref)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Any___type_6))))))
    (type $<classVTable>___type_33 (sub $<classVTable>___type_5 (struct (field (ref null $SpecialITable___type_2)))))
    (type $kotlin.wasm.internal.JsExternalBox___type_34 (sub $kotlin.Any___type_6 (struct (field (ref $<classVTable>___type_33)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut externref)))))
    (type $<classVTable>___type_35 (sub $<classVTable>___type_31 (struct (field (ref null $SpecialITable___type_2)) (field (ref null $____type_29)) (field (ref null $____type_30)) (field (ref null $____type_29)))))
    (type $kotlin.js.JsException___type_36 (sub $kotlin.Throwable___type_32 (struct (field (ref $<classVTable>___type_35)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Throwable___type_32))) (field (mut externref)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Any___type_6))) (field (mut externref)) (field (mut (ref null $kotlin.String___type_27))) (field (mut externref)))))
    (type $<classVTable>___type_37 (sub $<classVTable>___type_31 (struct (field (ref null $SpecialITable___type_2)) (field (ref null $____type_29)) (field (ref null $____type_30)))))
    (type $kotlin.Exception___type_38 (sub $kotlin.Throwable___type_32 (struct (field (ref $<classVTable>___type_37)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Throwable___type_32))) (field (mut externref)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Any___type_6))))))
    (type $<classVTable>___type_39 (sub $<classVTable>___type_37 (struct (field (ref null $SpecialITable___type_2)) (field (ref null $____type_29)) (field (ref null $____type_30)))))
    (type $kotlin.RuntimeException___type_40 (sub $kotlin.Exception___type_38 (struct (field (ref $<classVTable>___type_39)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Throwable___type_32))) (field (mut externref)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Any___type_6))))))
    (type $<classVTable>___type_41 (sub $<classVTable>___type_39 (struct (field (ref null $SpecialITable___type_2)) (field (ref null $____type_29)) (field (ref null $____type_30)))))
    (type $kotlin.IllegalArgumentException___type_42 (sub $kotlin.RuntimeException___type_40 (struct (field (ref $<classVTable>___type_41)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Throwable___type_32))) (field (mut externref)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Any___type_6))))))
    (type $<classVTable>___type_43 (sub $<classVTable>___type_39 (struct (field (ref null $SpecialITable___type_2)) (field (ref null $____type_29)) (field (ref null $____type_30)))))
    (rec
        (type $kotlin.ArithmeticException___type_44 (sub $kotlin.RuntimeException___type_40 (struct (field (ref $<classVTable>___type_43)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Throwable___type_32))) (field (mut externref)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Any___type_6))))))
        (type $mixin_1___type_45 (struct (field i31ref) (field i64))))
    (type $<classVTable>___type_46 (sub $<classVTable>___type_39 (struct (field (ref null $SpecialITable___type_2)) (field (ref null $____type_29)) (field (ref null $____type_30)))))
    (rec
        (type $kotlin.IndexOutOfBoundsException___type_47 (sub $kotlin.RuntimeException___type_40 (struct (field (ref $<classVTable>___type_46)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Throwable___type_32))) (field (mut externref)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Any___type_6))))))
        (type $mixin_2___type_48 (struct (field i31ref) (field f32))))
    (type $<classVTable>___type_49 (sub $<classVTable>___type_39 (struct (field (ref null $SpecialITable___type_2)) (field (ref null $____type_29)) (field (ref null $____type_30)))))
    (rec
        (type $kotlin.IllegalStateException___type_50 (sub $kotlin.RuntimeException___type_40 (struct (field (ref $<classVTable>___type_49)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Throwable___type_32))) (field (mut externref)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Any___type_6))))))
        (type $mixin_3___type_51 (struct (field i31ref) (field f64))))
    (type $<classVTable>___type_52 (sub $<classVTable>___type_39 (struct (field (ref null $SpecialITable___type_2)) (field (ref null $____type_29)) (field (ref null $____type_30)))))
    (rec
        (type $kotlin.ClassCastException___type_53 (sub $kotlin.RuntimeException___type_40 (struct (field (ref $<classVTable>___type_52)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Throwable___type_32))) (field (mut externref)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Any___type_6))))))
        (type $mixin_4___type_54 (struct (field i31ref) (field v128))))
    (type $<classVTable>___type_55 (sub $<classVTable>___type_39 (struct (field (ref null $SpecialITable___type_2)) (field (ref null $____type_29)) (field (ref null $____type_30)))))
    (rec
        (type $kotlin.NullPointerException___type_56 (sub $kotlin.RuntimeException___type_40 (struct (field (ref $<classVTable>___type_55)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Throwable___type_32))) (field (mut externref)) (field (mut (ref null $kotlin.String___type_27))) (field (mut (ref null $kotlin.Any___type_6))))))
        (type $mixin_5___type_57 (struct (field i31ref) (field i8))))
    (type $<classVTable>___type_58 (sub $<classVTable>___type_5 (struct (field (ref null $SpecialITable___type_2)))))
    (rec
        (type $kotlin.Unit___type_59 (sub $kotlin.Any___type_6 (struct (field (ref $<classVTable>___type_58)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)))))
        (type $mixin_2___type_60 (struct (field i31ref) (field f32))))
    (type $<classVTable>___type_61 (sub $<classVTable>___type_5 (struct (field (ref null $SpecialITable___type_2)))))
    (rec
        (type $kotlin.wasm.unsafe.Pointer___type_62 (sub $kotlin.Any___type_6 (struct (field (ref $<classVTable>___type_61)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut i32)))))
        (type $mixin_1___type_63 (struct (field i31ref) (field i64))))
    (type $____type_64 (func (param (ref null $kotlin.Any___type_6) i32) (result i32)))
    (type $<classVTable>___type_65 (sub $<classVTable>___type_5 (struct (field (ref null $SpecialITable___type_2)) (field (ref null $____type_64)))))
    (type $kotlin.wasm.unsafe.MemoryAllocator___type_66 (sub $kotlin.Any___type_6 (struct (field (ref $<classVTable>___type_65)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)))))
    (type $<classVTable>___type_67 (sub $<classVTable>___type_65 (struct (field (ref null $SpecialITable___type_2)) (field (ref null $____type_64)))))
    (type $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 (sub $kotlin.wasm.unsafe.MemoryAllocator___type_66 (struct (field (ref $<classVTable>___type_67)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)) (field (mut (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))) (field (mut i8)) (field (mut i8)) (field (mut i32)))))
    (type $____type_69 (func (param (ref null $kotlin.Any___type_6) (ref null $kotlin.Any___type_6)) (result (ref null $kotlin.Any___type_6))))
    (type $<classVTable>___type_70 (sub $<classVTable>___type_5 (struct (field (ref null $SpecialITable___type_2)) (field (ref null $____type_64)) (field (ref null $____type_69)))))
    (type $x$lambda___type_71 (sub $kotlin.Any___type_6 (struct (field (ref $<classVTable>___type_70)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)))))
    (type $<classVTable>___type_72 (sub $<classVTable>___type_5 (struct (field (ref null $SpecialITable___type_2)) (field (ref null $____type_64)) (field (ref null $____type_69)))))
    (rec
        (type $y$lambda___type_73 (sub $kotlin.Any___type_6 (struct (field (ref $<classVTable>___type_72)) (field (ref null $AnyArray___type_0)) (field (ref $RTTI___type_4)) (field (mut i32)))))
        (type $mixin_1___type_74 (struct (field i31ref) (field i64))))
    (type $<classITable>___type_75 (struct (field (ref null $____type_24))))
    (type $<classITable>___type_76 (struct))
    (type $<classITable>___type_77 (struct))
    (type $<classITable>___type_78 (struct (field (ref null $____type_69))))
    (type $____type_79 (func (param (ref null $kotlin.Number___type_8)) (result (ref null $kotlin.Number___type_8))))
    (type $____type_80 (func (param i32 i32) (result i32)))
    (type $____type_81 (func (param i32) (result i32)))
    (type $____type_82 (func (param (ref null $kotlin.Companion___type_10)) (result (ref null $kotlin.Companion___type_10))))
    (type $____type_83 (func (param (ref null $kotlin.Array___type_15) i32) (result (ref null $kotlin.Array___type_15))))
    (type $____type_84 (func (param (ref null $kotlin.Array___type_15) i32) (result (ref null $kotlin.Any___type_6))))
    (type $____type_85 (func (param (ref null $kotlin.Array___type_15) i32 (ref null $kotlin.Any___type_6))))
    (type $____type_86 (func (param (ref null $kotlin.Companion___type_17)) (result (ref null $kotlin.Companion___type_17))))
    (type $____type_87 (func (param (ref null $kotlin.Companion___type_22)) (result (ref null $kotlin.Companion___type_22))))
    (type $____type_88 (func (param (ref null $kotlin.String___type_27)) (result (ref null $kotlin.String___type_27))))
    (type $____type_89 (func (param (ref null $kotlin.String___type_27)) (result (ref null $kotlin.wasm.internal.WasmCharArray___type_26))))
    (type $____type_90 (func (param (ref null $kotlin.String___type_27))))
    (type $____type_91 (func (param i32 i32 i32) (result (ref null $kotlin.String___type_27))))
    (type $____type_92 (func (param i32 i32 i32 (ref null $kotlin.wasm.internal.WasmCharArray___type_26))))
    (type $____type_93 (func (param (ref null $kotlin.wasm.internal.WasmCharArray___type_26) i32 i32 i32)))
    (type $____type_94 (func (param i32 i32)))
    (type $____type_95 (func (param)))
    (type $____type_96 (func (param (ref null struct)) (result (ref null $kotlin.String___type_27))))
    (type $____type_97 (func (param (ref null $kotlin.Any___type_6) i64) (result i32)))
    (type $____type_98 (func (param (ref null $kotlin.wasm.internal.WasmLongImmutableArray___type_28) i64) (result i32)))
    (type $____type_99 (func (param (ref null $kotlin.Any___type_6) i64) (result anyref)))
    (type $____type_100 (func (param (ref null $kotlin.Throwable___type_32) (ref null $kotlin.String___type_27) (ref null $kotlin.Throwable___type_32) externref) (result (ref null $kotlin.Throwable___type_32))))
    (type $____type_101 (func (param (ref null $kotlin.Throwable___type_32) (ref null $kotlin.String___type_27) (ref null $kotlin.Throwable___type_32)) (result (ref null $kotlin.Throwable___type_32))))
    (type $____type_102 (func (param (ref null $kotlin.Throwable___type_32) (ref null $kotlin.String___type_27)) (result (ref null $kotlin.Throwable___type_32))))
    (type $____type_103 (func (param (ref null $kotlin.Throwable___type_32)) (result (ref null $kotlin.Throwable___type_32))))
    (type $____type_104 (func (param) (result externref)))
    (type $____type_105 (func (param (ref null $kotlin.Throwable___type_32))))
    (type $____type_106 (func (param (ref null $kotlin.String___type_27) (ref null $kotlin.String___type_27) externref)))
    (type $____type_107 (func (param externref externref externref) (result nullref)))
    (type $____type_108 (func (param externref) (result (ref null $kotlin.js.JsException___type_36))))
    (type $____type_109 (func (param externref) (result (ref null $kotlin.String___type_27))))
    (type $____type_110 (func (param externref) (result i32)))
    (type $____type_111 (func (param externref i32 i32 i32)))
    (type $____type_112 (func (param (ref null $kotlin.String___type_27)) (result externref)))
    (type $____type_113 (func (param i32 i32 externref) (result externref)))
    (type $____type_114 (func (param externref) (result externref)))
    (type $____type_115 (func (param externref) (result (ref null $kotlin.Any___type_6))))
    (type $____type_116 (func (param externref externref) (result externref)))
    (type $____type_117 (func (param (ref null $kotlin.js.JsException___type_36) externref) (result (ref null $kotlin.js.JsException___type_36))))
    (type $____type_118 (func (param (ref null $kotlin.IllegalArgumentException___type_42) (ref null $kotlin.String___type_27)) (result (ref null $kotlin.IllegalArgumentException___type_42))))
    (type $____type_119 (func (param (ref null $kotlin.IllegalArgumentException___type_42)) (result (ref null $kotlin.IllegalArgumentException___type_42))))
    (type $____type_120 (func (param (ref null $kotlin.ArithmeticException___type_44) (ref null $kotlin.String___type_27)) (result (ref null $kotlin.ArithmeticException___type_44))))
    (type $____type_121 (func (param (ref null $kotlin.ArithmeticException___type_44)) (result (ref null $kotlin.ArithmeticException___type_44))))
    (type $____type_122 (func (param (ref null $kotlin.IndexOutOfBoundsException___type_47)) (result (ref null $kotlin.IndexOutOfBoundsException___type_47))))
    (type $____type_123 (func (param (ref null $kotlin.RuntimeException___type_40)) (result (ref null $kotlin.RuntimeException___type_40))))
    (type $____type_124 (func (param (ref null $kotlin.RuntimeException___type_40) (ref null $kotlin.String___type_27)) (result (ref null $kotlin.RuntimeException___type_40))))
    (type $____type_125 (func (param (ref null $kotlin.Exception___type_38)) (result (ref null $kotlin.Exception___type_38))))
    (type $____type_126 (func (param (ref null $kotlin.Exception___type_38) (ref null $kotlin.String___type_27)) (result (ref null $kotlin.Exception___type_38))))
    (type $____type_127 (func (param (ref null $kotlin.IllegalStateException___type_50) (ref null $kotlin.String___type_27)) (result (ref null $kotlin.IllegalStateException___type_50))))
    (type $____type_128 (func (param (ref null $kotlin.IllegalStateException___type_50)) (result (ref null $kotlin.IllegalStateException___type_50))))
    (type $____type_129 (func (param (ref null $kotlin.ClassCastException___type_53)) (result (ref null $kotlin.ClassCastException___type_53))))
    (type $____type_130 (func (param (ref null $kotlin.NullPointerException___type_56)) (result (ref null $kotlin.NullPointerException___type_56))))
    (type $____type_131 (func (param (ref null $kotlin.Unit___type_59)) (result (ref null $kotlin.Unit___type_59))))
    (type $____type_132 (func (param) (result (ref null $kotlin.Unit___type_59))))
    (type $____type_133 (func (param (ref null $kotlin.wasm.unsafe.MemoryAllocator___type_66)) (result (ref null $kotlin.wasm.unsafe.MemoryAllocator___type_66))))
    (type $____type_134 (func (param (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68) i32 (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68)) (result (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))))
    (type $____type_135 (func (param (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68)) (result (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))))
    (type $____type_136 (func (param (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))))
    (type $____type_137 (func (param) (result (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))))
    (type $____type_138 (func (param) (result (ref null $kotlin.Any___type_6))))
    (type $____type_139 (func (param i32 (ref null $kotlin.Any___type_6)) (result i32)))
    (type $____type_140 (func (param) (result (ref null $kotlin.String___type_27))))
    (type $____type_141 (func (param (ref null $x$lambda___type_71)) (result (ref null $x$lambda___type_71))))
    (type $____type_142 (func (param (ref null $y$lambda___type_73)) (result (ref null $y$lambda___type_73))))
    (type $____type_143 (func (param)))
    (type $____type_144 (func (param (ref null $kotlin.Throwable___type_32))))
    (func $kotlin.captureStackTrace___fun_0 (import "js_code" "kotlin.captureStackTrace") (type $____type_104))
    (func $kotlin.wasm.internal.throwJsError___fun_1 (import "js_code" "kotlin.wasm.internal.throwJsError") (type $____type_107))
    (func $kotlin.wasm.internal.stringLength___fun_2 (import "js_code" "kotlin.wasm.internal.stringLength") (type $____type_110))
    (func $kotlin.wasm.internal.jsExportStringToWasm___fun_3 (import "js_code" "kotlin.wasm.internal.jsExportStringToWasm") (type $____type_111))
    (func $kotlin.wasm.internal.importStringFromWasm___fun_4 (import "js_code" "kotlin.wasm.internal.importStringFromWasm") (type $____type_113))
    (func $kotlin.wasm.internal.getJsEmptyString___fun_5 (import "js_code" "kotlin.wasm.internal.getJsEmptyString") (type $____type_104))
    (func $kotlin.wasm.internal.isNullish___fun_6 (import "js_code" "kotlin.wasm.internal.isNullish") (type $____type_110))
    (func $kotlin.wasm.internal.getCachedJsObject_$external_fun___fun_7 (import "js_code" "kotlin.wasm.internal.getCachedJsObject_$external_fun") (type $____type_116))
    (func $kotlin.js.stackPlaceHolder_js_code___fun_8 (import "js_code" "kotlin.js.stackPlaceHolder_js_code") (type $____type_104))
    (func $kotlin.js.message_$external_prop_getter___fun_9 (import "js_code" "kotlin.js.message_$external_prop_getter") (type $____type_114))
    (func $kotlin.js.stack_$external_prop_getter___fun_10 (import "js_code" "kotlin.js.stack_$external_prop_getter") (type $____type_114))
    (func $kotlin.js.JsError_$external_class_instanceof___fun_11 (import "js_code" "kotlin.js.JsError_$external_class_instanceof") (type $____type_110))
    (func $kotlin.Number.<init>___fun_12 (type $____type_79)
        (param $0_<this> (ref null $kotlin.Number___type_8)) (result (ref null $kotlin.Number___type_8))
        nop
        local.get $0_<this>
        return)
    (func $kotlin.ranges.coerceAtMost___fun_13 (type $____type_80)
        (param $0_<this> i32)
        (param $1_maximumValue i32) (result i32)  ;; type: kotlin.Int  ;; type: kotlin.Int
        local.get $0_<this>  ;; type: kotlin.Int
        local.get $1_maximumValue
        i32.gt_s  ;; type: kotlin.Int
        if (result i32)
            local.get $1_maximumValue
        else
            local.get $0_<this>
        end
        return)
    (func $kotlin.<UInt__<init>-impl>___fun_14 (type $____type_81)
        (param $0_data i32) (result i32)  ;; type: kotlin.Int
        local.get $0_data
        return)
    (func $kotlin.<UInt__<get-data>-impl>___fun_15 (type $____type_81)
        (param $0_$this i32) (result i32)  ;; type: kotlin.UInt
        local.get $0_$this
        return)
    (func $kotlin.Companion.<init>___fun_16 (type $____type_82)
        (param $0_<this> (ref null $kotlin.Companion___type_10)) (result (ref null $kotlin.Companion___type_10))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_10
            ref.null none
            global.get $kotlin.Companion_rtti___g_46
            i32.const 0
            i32.const 0
            i32.const 0
            i32.const 0
            
            i32.const 0
            struct.new $kotlin.Companion___type_10  ;; type: kotlin.Companion
            local.set $0_<this>  ;; name: MIN_VALUE, type: kotlin.UInt
        end
        local.get $0_<this>  ;; type: kotlin.Companion
        i32.const 0  ;; name: MAX_VALUE, type: kotlin.UInt
        struct.set $kotlin.Companion___type_10 4
        local.get $0_<this>  ;; type: kotlin.Companion
        i32.const -1  ;; name: SIZE_BYTES, type: kotlin.Int
        struct.set $kotlin.Companion___type_10 5
        local.get $0_<this>  ;; type: kotlin.Companion
        i32.const 4  ;; name: SIZE_BITS, type: kotlin.Int
        struct.set $kotlin.Companion___type_10 6
        local.get $0_<this>
        i32.const 32
        struct.set $kotlin.Companion___type_10 7
        nop
        local.get $0_<this>
        return)
    (func $kotlin.Array.<init>___fun_17 (type $____type_83)
        (param $0_<this> (ref null $kotlin.Array___type_15))
        (param $1_size i32) (result (ref null $kotlin.Array___type_15))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_13
            ref.null none
            global.get $kotlin.Array_rtti___g_48
            i32.const 0
            
            ref.null $kotlin.wasm.internal.WasmAnyArray___type_14
            struct.new $kotlin.Array___type_15  ;; type: kotlin.Int
            local.set $0_<this>
        end
        local.get $1_size
        
        ;; const string: "Negative array size"
        i32.const 0
        i32.lt_s
        if
            ref.null none
            
            i32.const 6
            i32.const 54
            i32.const 19
            call $kotlin.stringLiteral___fun_29  ;; type: kotlin.Array<T of kotlin.Array>
            call $kotlin.IllegalArgumentException.<init>___fun_70  ;; type: kotlin.Int
            throw 0  ;; @WasmArrayOf ctor call: kotlin.wasm.internal.WasmAnyArray
        end  ;; name: storage, type: kotlin.wasm.internal.WasmAnyArray
        local.get $0_<this>
        local.get $1_size
        array.new_default $kotlin.wasm.internal.WasmAnyArray___type_14
        struct.set $kotlin.Array___type_15 4
        nop
        local.get $0_<this>
        return)
    (func $kotlin.Array.get___fun_18 (type $____type_84)
        (param $0_<this> (ref null $kotlin.Array___type_15))
        (param $1_index i32) (result (ref null $kotlin.Any___type_6))  ;; type: kotlin.Int  ;; type: kotlin.Array<T of kotlin.Array>  ;; name: storage, type: kotlin.wasm.internal.WasmAnyArray
        local.get $1_index
        local.get $0_<this>
        struct.get $kotlin.Array___type_15 4  ;; type: kotlin.Array<T of kotlin.Array>  ;; name: storage, type: kotlin.wasm.internal.WasmAnyArray
        array.len  ;; type: kotlin.Int
        call $kotlin.wasm.internal.rangeCheck___fun_32
        local.get $0_<this>
        struct.get $kotlin.Array___type_15 4
        local.get $1_index
        array.get $kotlin.wasm.internal.WasmAnyArray___type_14
        return)
    (func $kotlin.Array.set___fun_19 (type $____type_85)
        (param $0_<this> (ref null $kotlin.Array___type_15))
        (param $1_index i32)
        (param $2_value (ref null $kotlin.Any___type_6))  ;; type: kotlin.Int  ;; type: kotlin.Array<T of kotlin.Array>  ;; name: storage, type: kotlin.wasm.internal.WasmAnyArray
        local.get $1_index
        local.get $0_<this>
        struct.get $kotlin.Array___type_15 4  ;; type: kotlin.Array<T of kotlin.Array>  ;; name: storage, type: kotlin.wasm.internal.WasmAnyArray
        array.len  ;; type: kotlin.Int
        call $kotlin.wasm.internal.rangeCheck___fun_32  ;; type: T of kotlin.Array
        local.get $0_<this>
        struct.get $kotlin.Array___type_15 4
        local.get $1_index
        local.get $2_value
        array.set $kotlin.wasm.internal.WasmAnyArray___type_14
        nop)
    (func $kotlin.Companion.<init>___fun_20 (type $____type_86)
        (param $0_<this> (ref null $kotlin.Companion___type_17)) (result (ref null $kotlin.Companion___type_17))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_14
            ref.null none
            global.get $kotlin.Companion_rtti___g_49
            i32.const 0
            i32.const 0
            i32.const 0
            i32.const 0
            
            i32.const 0
            struct.new $kotlin.Companion___type_17  ;; type: kotlin.Companion
            local.set $0_<this>  ;; name: MIN_VALUE, type: kotlin.Int
        end
        local.get $0_<this>  ;; type: kotlin.Companion
        i32.const -2147483648  ;; name: MAX_VALUE, type: kotlin.Int
        struct.set $kotlin.Companion___type_17 4
        local.get $0_<this>  ;; type: kotlin.Companion
        i32.const 2147483647  ;; name: SIZE_BYTES, type: kotlin.Int
        struct.set $kotlin.Companion___type_17 5
        local.get $0_<this>  ;; type: kotlin.Companion
        i32.const 4  ;; name: SIZE_BITS, type: kotlin.Int
        struct.set $kotlin.Companion___type_17 6
        local.get $0_<this>
        i32.const 32
        struct.set $kotlin.Companion___type_17 7
        nop
        local.get $0_<this>
        return)
    (func $kotlin.Int__div-impl___fun_21 (type $____type_80)
        (param $0_$this i32)
        (param $1_other i32) (result i32)
        (local $2_tmp0_subject i32)
        (local $3_TABLE_SWITCH_SELECTOR i32)  ;; type: kotlin.Int  ;; type: kotlin.Int
        local.get $1_other
        local.tee $2_tmp0_subject
        local.set $3_TABLE_SWITCH_SELECTOR
        block (result i32)
            block (result i32)
                block (result i32)
                    block (result i32)
                        i32.const 0
                        local.get $3_TABLE_SWITCH_SELECTOR
                        i32.const -1
                        i32.sub  ;; type: kotlin.Int
                        br_table 0 1 2
                    end
                    drop
                    local.get $0_$this
                    i32.const -2147483648
                    i32.eq  ;; type: kotlin.Int
                    if (result i32)  ;; type: kotlin.Int
                        i32.const -2147483648
                    else
                        local.get $0_$this
                        local.get $1_other
                        i32.div_s
                    end
                    
                    ;; const string: "Division by zero"
                    br 2
                end
                drop
                ref.null none
                
                i32.const 7
                i32.const 92
                i32.const 16
                call $kotlin.stringLiteral___fun_29
                call $kotlin.ArithmeticException.<init>___fun_72  ;; type: kotlin.Int
                throw 0  ;; type: kotlin.Int
            end
            drop
            local.get $0_$this
            local.get $1_other
            i32.div_s
        end
        return)
    (func $kotlin.Int__rem-impl___fun_22 (type $____type_80)
        (param $0_$this i32)
        (param $1_other i32) (result i32)
        (local $2_tmp0_subject i32)  ;; type: kotlin.Int  ;; type: kotlin.Int
        local.get $1_other
        local.tee $2_tmp0_subject
        
        ;; const string: "Division by zero"
        i32.const 0
        i32.eq
        if (result i32)
            ref.null none
            
            i32.const 7
            i32.const 92
            i32.const 16
            call $kotlin.stringLiteral___fun_29  ;; type: kotlin.Int
            call $kotlin.ArithmeticException.<init>___fun_72  ;; type: kotlin.Int
            throw 0
        else
            local.get $0_$this
            local.get $1_other
            i32.rem_s
        end
        return)
    (func $kotlin.Int__toChar-impl___fun_23 (type $____type_81)
        (param $0_$this i32) (result i32)  ;; type: kotlin.Int
        local.get $0_$this
        i32.const 65535
        i32.and
        return)
    (func $kotlin.Companion.<init>___fun_24 (type $____type_87)
        (param $0_<this> (ref null $kotlin.Companion___type_22)) (result (ref null $kotlin.Companion___type_22))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_16
            ref.null none
            global.get $kotlin.Companion_rtti___g_50
            
            i32.const 0
            struct.new $kotlin.Companion___type_22
            local.set $0_<this>
        end
        nop
        local.get $0_<this>
        return)
    (func $kotlin.access$<get-leftIfInSum>___fun_25 (type $____type_88)
        (param $0_$this (ref null $kotlin.String___type_27)) (result (ref null $kotlin.String___type_27))  ;; type: kotlin.String  ;; name: leftIfInSum, type: kotlin.String?
        local.get $0_$this
        struct.get $kotlin.String___type_27 4
        return)
    (func $kotlin.access$<get-_chars>___fun_26 (type $____type_89)
        (param $0_$this (ref null $kotlin.String___type_27)) (result (ref null $kotlin.wasm.internal.WasmCharArray___type_26))  ;; type: kotlin.String  ;; name: _chars, type: kotlin.wasm.internal.WasmCharArray
        local.get $0_$this
        struct.get $kotlin.String___type_27 6
        return)
    (func $kotlin.String.<get-length>___fun_27 (type $____type_24)
        (param $0_<this> (ref null $kotlin.Any___type_6)) (result i32)
        (local $1_tmp0_<this> (ref null $kotlin.String___type_27))  ;; type: kotlin.Any  ;; type: kotlin.String
        local.get $0_<this>  ;; name: length, type: kotlin.Int
        ref.cast null $kotlin.String___type_27
        local.tee $1_tmp0_<this>
        struct.get $kotlin.String___type_27 5
        return)
    (func $kotlin.String.foldChars___fun_28 (type $____type_90)
        (param $0_<this> (ref null $kotlin.String___type_27))
        (local $1_stringLength i32)
        (local $2_newArray (ref null $kotlin.wasm.internal.WasmCharArray___type_26))
        (local $3_currentStartIndex i32)
        (local $4_currentLeftString (ref null $kotlin.String___type_27))
        (local $5_currentLeftStringChars (ref null $kotlin.wasm.internal.WasmCharArray___type_26))
        (local $6_currentLeftStringLen i32)
        (local $7_tmp0 (ref null $kotlin.wasm.internal.WasmCharArray___type_26))
        (local $8_tmp2 (ref null $kotlin.wasm.internal.WasmCharArray___type_26))
        (local $9_tmp4 i32)
        (local $10_tmp6 i32)
        (local $11_tmp8 i32)
        (local $12_source (ref null $kotlin.wasm.internal.WasmCharArray___type_26))
        (local $13_destination (ref null $kotlin.wasm.internal.WasmCharArray___type_26))
        (local $14_sourceIndex i32)
        (local $15_destinationIndex i32)
        (local $16_length i32)
        (local $17_tmp0 i32)
        (local $18_value i32)  ;; type: kotlin.String  ;; name: length, type: kotlin.Int  ;; type: kotlin.Int
        local.get $0_<this>  ;; @WasmArrayOf ctor call: kotlin.wasm.internal.WasmCharArray
        struct.get $kotlin.String___type_27 5  ;; type: kotlin.Int
        local.tee $1_stringLength
        array.new_default $kotlin.wasm.internal.WasmCharArray___type_26  ;; type: kotlin.String
        local.set $2_newArray
        local.get $1_stringLength
        local.set $3_currentStartIndex
        local.get $0_<this>  ;; type: kotlin.String?
        local.set $4_currentLeftString
        loop
            block
                local.get $4_currentLeftString
                ref.is_null  ;; type: kotlin.String?
                i32.eqz  ;; name: _chars, type: kotlin.wasm.internal.WasmCharArray
                i32.eqz
                br_if 0  ;; type: kotlin.wasm.internal.WasmCharArray
                local.get $4_currentLeftString
                struct.get $kotlin.String___type_27 6  ;; type: kotlin.Int
                local.tee $5_currentLeftStringChars  ;; type: kotlin.Int
                array.len
                local.set $6_currentLeftStringLen  ;; type: kotlin.Int
                local.get $3_currentStartIndex
                local.get $6_currentLeftStringLen  ;; type: kotlin.wasm.internal.WasmCharArray
                i32.sub  ;; type: kotlin.wasm.internal.WasmCharArray
                local.set $3_currentStartIndex
                local.get $5_currentLeftStringChars
                local.set $7_tmp0
                local.get $2_newArray  ;; type: kotlin.Int
                local.set $8_tmp2
                i32.const 0  ;; type: kotlin.Int
                local.set $9_tmp4
                local.get $3_currentStartIndex
                
                ;; Inlined call of `kotlin.wasm.internal.copyWasmArray`
                local.set $10_tmp6
                local.get $6_currentLeftStringLen  ;; type: kotlin.wasm.internal.WasmCharArray
                local.set $11_tmp8
                block (result (ref null $kotlin.Unit___type_59))  ;; type: kotlin.wasm.internal.WasmCharArray
                    nop
                    local.get $7_tmp0  ;; type: kotlin.Int
                    local.set $12_source
                    local.get $8_tmp2  ;; type: kotlin.Int
                    local.set $13_destination
                    local.get $9_tmp4  ;; type: kotlin.Int
                    local.set $14_sourceIndex
                    local.get $10_tmp6  ;; type: kotlin.wasm.internal.WasmCharArray
                    local.set $15_destinationIndex  ;; type: kotlin.Int
                    local.get $11_tmp8  ;; type: kotlin.wasm.internal.WasmCharArray
                    local.set $16_length  ;; type: kotlin.Int
                    local.get $13_destination  ;; type: kotlin.Int
                    local.get $15_destinationIndex
                    local.get $12_source
                    local.get $14_sourceIndex  ;; type: kotlin.Unit?
                    local.get $16_length
                    
                    array.copy $kotlin.wasm.internal.WasmCharArray___type_26 $kotlin.wasm.internal.WasmCharArray___type_26
                    global.get $kotlin.Unit_instance___g_8  ;; type: kotlin.String?
                    br 0  ;; name: leftIfInSum, type: kotlin.String?
                end  ;; type: kotlin.String?
                drop
                local.get $4_currentLeftString
                struct.get $kotlin.String___type_27 4
                local.set $4_currentLeftString
                br 1  ;; type: kotlin.Int
            end
        end
        local.get $3_currentStartIndex
        
        ;; Inlined call of `kotlin.check`
        i32.const 0
        i32.eq  ;; type: kotlin.Boolean
        local.set $17_tmp0
        block (result (ref null $kotlin.Unit___type_59))  ;; type: kotlin.Boolean
            nop
            local.get $17_tmp0
            
            ;; const string: "Check failed."
            local.tee $18_value
            i32.eqz
            if
                ref.null none
                
                i32.const 10
                i32.const 142
                i32.const 13
                call $kotlin.stringLiteral___fun_29  ;; type: kotlin.Unit?
                call $kotlin.IllegalStateException.<init>___fun_82
                throw 0
                
            end
            global.get $kotlin.Unit_instance___g_8  ;; type: kotlin.String
            br 0  ;; type: kotlin.wasm.internal.WasmCharArray
        end  ;; name: _chars, type: kotlin.wasm.internal.WasmCharArray
        drop
        local.get $0_<this>  ;; type: kotlin.String
        local.get $2_newArray  ;; name: leftIfInSum, type: kotlin.String?
        struct.set $kotlin.String___type_27 6
        local.get $0_<this>
        ref.null none
        struct.set $kotlin.String___type_27 4
        nop)
    (func $kotlin.stringLiteral___fun_29 (type $____type_91)
        (param $0_poolId i32)
        (param $1_startAddress i32)
        (param $2_length i32) (result (ref null $kotlin.String___type_27))
        (local $3_cached (ref null $kotlin.String___type_27))
        (local $4_chars (ref null $kotlin.wasm.internal.WasmCharArray___type_26))
        (local $5_newString (ref null $kotlin.String___type_27))  ;; type: kotlin.Array<kotlin.String?>  ;; type: kotlin.Int
        global.get $kotlin.wasm.internal.stringPool___g_4
        local.get $0_poolId  ;; type: kotlin.String?
        call $kotlin.Array.get___fun_18
        ref.cast null $kotlin.String___type_27
        local.tee $3_cached  ;; type: kotlin.String?
        ref.is_null
        i32.eqz
        if  ;; type: kotlin.Int
            local.get $3_cached  ;; type: kotlin.Int
            return
        end
        local.get $1_startAddress
        
        ;; Any parameters
        local.get $2_length
        array.new_data $kotlin.wasm.internal.WasmCharArray___type_26 0
        ref.cast null $kotlin.wasm.internal.WasmCharArray___type_26
        local.set $4_chars
        
        global.get $<classVTable>___g_17
        global.get $<classITable>___g_34  ;; type: kotlin.Int
        global.get $kotlin.String_rtti___g_51  ;; type: kotlin.wasm.internal.WasmCharArray
        i32.const 0  ;; @WasmPrimitiveConstructor ctor call: kotlin.String
        ref.null none
        local.get $2_length  ;; type: kotlin.Array<kotlin.String?>
        local.get $4_chars  ;; type: kotlin.Int
        struct.new $kotlin.String___type_27  ;; type: kotlin.String
        local.set $5_newString
        global.get $kotlin.wasm.internal.stringPool___g_4
        local.get $0_poolId  ;; type: kotlin.String
        local.get $5_newString
        call $kotlin.Array.set___fun_19
        local.get $5_newString
        return)
    (func $kotlin.wasm.internal.unsafeRawMemoryToWasmCharArray___fun_30 (type $____type_92)
        (param $0_srcAddr i32)
        (param $1_dstOffset i32)
        (param $2_dstLength i32)
        (param $3_dst (ref null $kotlin.wasm.internal.WasmCharArray___type_26))
        (local $4_curAddr i32)
        (local $5_srcAddrEndOffset i32)
        (local $6_dstIndex i32)
        (local $7_char i32)
        (local $8_<unary> i32)
        (local $9_tmp0 i32)
        (local $10_this i32)  ;; type: kotlin.Int  ;; type: kotlin.Int  ;; type: kotlin.Int
        local.get $0_srcAddr
        local.set $4_curAddr
        local.get $0_srcAddr
        local.get $2_dstLength
        i32.const 2  ;; type: kotlin.Int
        i32.mul
        i32.add
        local.set $5_srcAddrEndOffset
        local.get $1_dstOffset  ;; type: kotlin.Int
        local.set $6_dstIndex  ;; type: kotlin.Int
        loop
            block
                local.get $4_curAddr
                local.get $5_srcAddrEndOffset  ;; type: kotlin.Int
                i32.lt_s
                i32.eqz
                br_if 0
                local.get $4_curAddr  ;; type: kotlin.wasm.internal.WasmCharArray
                i32.load16_u align=1  ;; type: kotlin.Int
                call $kotlin.Int__toChar-impl___fun_23  ;; type: kotlin.Char
                local.set $7_char
                local.get $3_dst
                local.get $6_dstIndex  ;; type: kotlin.Int
                local.get $7_char
                array.set $kotlin.wasm.internal.WasmCharArray___type_26  ;; type: kotlin.Int
                local.get $4_curAddr
                i32.const 2  ;; type: kotlin.Int
                i32.add  ;; type: kotlin.Int
                local.set $4_curAddr
                local.get $6_dstIndex
                
                ;; Inlined call of `kotlin.Int.inc`
                local.tee $8_<unary>  ;; type: kotlin.Int
                local.set $9_tmp0
                block (result i32)  ;; type: kotlin.Int
                    nop
                    local.get $9_tmp0
                    local.tee $10_this
                    
                    i32.const 1  ;; type: kotlin.Int
                    i32.add
                    br 0  ;; type: kotlin.Int
                end
                local.set $6_dstIndex
                br 1
            end
        end
        nop)
    (func $kotlin.wasm.internal.unsafeWasmCharArrayToRawMemory___fun_31 (type $____type_93)
        (param $0_src (ref null $kotlin.wasm.internal.WasmCharArray___type_26))
        (param $1_srcOffset i32)
        (param $2_srcLength i32)
        (param $3_dstAddr i32)
        (local $4_curAddr i32)
        (local $5_srcEndOffset i32)
        (local $6_srcIndex i32)
        (local $7_<unary> i32)
        (local $8_tmp0 i32)
        (local $9_this i32)  ;; type: kotlin.Int  ;; type: kotlin.Int  ;; type: kotlin.Int
        local.get $3_dstAddr
        local.set $4_curAddr
        local.get $1_srcOffset  ;; type: kotlin.Int
        local.get $2_srcLength
        i32.add
        local.set $5_srcEndOffset
        local.get $1_srcOffset  ;; type: kotlin.Int
        local.set $6_srcIndex  ;; type: kotlin.Int
        loop
            block
                local.get $6_srcIndex
                local.get $5_srcEndOffset  ;; type: kotlin.Int
                i32.lt_s  ;; type: kotlin.wasm.internal.WasmCharArray
                i32.eqz  ;; type: kotlin.Int
                br_if 0
                local.get $4_curAddr
                local.get $0_src
                local.get $6_srcIndex  ;; type: kotlin.Int
                array.get_u $kotlin.wasm.internal.WasmCharArray___type_26
                i32.store16 align=1  ;; type: kotlin.Int
                local.get $4_curAddr
                i32.const 2  ;; type: kotlin.Int
                i32.add  ;; type: kotlin.Int
                local.set $4_curAddr
                local.get $6_srcIndex
                
                ;; Inlined call of `kotlin.Int.inc`
                local.tee $7_<unary>  ;; type: kotlin.Int
                local.set $8_tmp0
                block (result i32)  ;; type: kotlin.Int
                    nop
                    local.get $8_tmp0
                    local.tee $9_this
                    
                    i32.const 1  ;; type: kotlin.Int
                    i32.add
                    br 0  ;; type: kotlin.Int
                end
                local.set $6_srcIndex
                br 1
            end
        end
        nop)
    (func $kotlin.wasm.internal.rangeCheck___fun_32 (type $____type_94)
        (param $0_index i32)
        (param $1_size i32)  ;; type: kotlin.Int
        local.get $0_index
        i32.const 0
        i32.lt_s  ;; type: kotlin.Int
        if (result i32)  ;; type: kotlin.Int
            i32.const 1
        else
            local.get $0_index
            local.get $1_size
            i32.ge_s
        end
        if
            ref.null none
            call $kotlin.IndexOutOfBoundsException.<init>___fun_74
            throw 0
        end
        nop)
    (func $kotlin.wasm.internal.THROW_NPE___fun_33 (type $____type_95)
        ref.null none
        call $kotlin.NullPointerException.<init>___fun_86
        throw 0)
    (func $kotlin.wasm.internal.THROW_CCE___fun_34 (type $____type_95)
        ref.null none
        call $kotlin.ClassCastException.<init>___fun_84
        throw 0)
    (func $kotlin.wasm.internal.getSimpleName___fun_35 (type $____type_96)
        (param $0_rtti (ref null struct)) (result (ref null $kotlin.String___type_27))
        (local $1_tmp0_startAddress i32)
        (local $2_tmp1_length i32)
        (local $3_tmp2_poolId i32)  ;; type: kotlin.wasm.internal.reftypes.structref
        local.get $0_rtti  ;; type: kotlin.wasm.internal.reftypes.structref
        ref.cast $RTTI___type_4
        struct.get $RTTI___type_4 5
        local.set $1_tmp0_startAddress
        local.get $0_rtti  ;; type: kotlin.wasm.internal.reftypes.structref
        ref.cast $RTTI___type_4
        struct.get $RTTI___type_4 6
        local.set $2_tmp1_length
        local.get $0_rtti  ;; type: kotlin.Int
        ref.cast $RTTI___type_4  ;; type: kotlin.Int
        struct.get $RTTI___type_4 7  ;; type: kotlin.Int
        local.tee $3_tmp2_poolId
        local.get $1_tmp0_startAddress
        local.get $2_tmp1_length
        call $kotlin.stringLiteral___fun_29
        return)
    (func $kotlin.wasm.internal.isSupportedInterface___fun_36 (type $____type_97)
        (param $0_obj (ref null $kotlin.Any___type_6))
        (param $1_interfaceId i64) (result i32)
        (local $2_interfaceArray (ref null $kotlin.wasm.internal.WasmLongImmutableArray___type_28))
        (local $3_tmp0_elvis_lhs (ref null $kotlin.wasm.internal.WasmLongImmutableArray___type_28))  ;; type: kotlin.Any
        local.get $0_obj  ;; type: kotlin.wasm.internal.WasmLongImmutableArray?
        struct.get $kotlin.Any___type_6 2
        struct.get $RTTI___type_4 0
        local.tee $3_tmp0_elvis_lhs
        ref.is_null
        if (result (ref null $kotlin.wasm.internal.WasmLongImmutableArray___type_28))  ;; type: kotlin.wasm.internal.WasmLongImmutableArray?
            i32.const 0
            return
        else  ;; type: kotlin.wasm.internal.WasmLongImmutableArray
            local.get $3_tmp0_elvis_lhs  ;; type: kotlin.Long
        end
        local.tee $2_interfaceArray
        local.get $1_interfaceId
        call $kotlin.wasm.internal.wasmArrayAnyIndexOfValue___fun_37
        i32.const -1
        i32.eq
        i32.eqz
        return)
    (func $kotlin.wasm.internal.wasmArrayAnyIndexOfValue___fun_37 (type $____type_98)
        (param $0_array (ref null $kotlin.wasm.internal.WasmLongImmutableArray___type_28))
        (param $1_value i64) (result i32)
        (local $2_arraySize i32)
        (local $3_index i32)
        (local $4_supportedInterface i64)
        (local $5_<unary> i32)
        (local $6_tmp0 i32)
        (local $7_this i32)  ;; type: kotlin.wasm.internal.WasmLongImmutableArray
        local.get $0_array
        array.len
        local.set $2_arraySize
        i32.const 0  ;; type: kotlin.Int
        local.set $3_index  ;; type: kotlin.Int
        loop
            block
                local.get $3_index
                local.get $2_arraySize  ;; type: kotlin.wasm.internal.WasmLongImmutableArray
                i32.lt_s  ;; type: kotlin.Int
                i32.eqz
                br_if 0
                local.get $0_array  ;; type: kotlin.Long
                local.get $3_index  ;; type: kotlin.Long
                array.get $kotlin.wasm.internal.WasmLongImmutableArray___type_28
                local.tee $4_supportedInterface  ;; type: kotlin.Int
                local.get $1_value
                i64.eq
                if  ;; type: kotlin.Int
                    local.get $3_index
                    return  ;; type: kotlin.Int
                end
                local.get $3_index
                
                ;; Inlined call of `kotlin.Int.inc`
                local.tee $5_<unary>  ;; type: kotlin.Int
                local.set $6_tmp0
                block (result i32)  ;; type: kotlin.Int
                    nop
                    local.get $6_tmp0
                    local.tee $7_this
                    
                    i32.const 1  ;; type: kotlin.Int
                    i32.add
                    br 0  ;; type: kotlin.Int
                end
                local.set $3_index
                br 1
            end
        end
        i32.const -1
        return)
    (func $kotlin.wasm.internal.getInterfaceVTable___fun_38 (type $____type_99)
        (param $0_obj (ref null $kotlin.Any___type_6))
        (param $1_interfaceId i64) (result anyref)
        local.get $0_obj
        struct.get $kotlin.Any___type_6 1
        local.get $0_obj
        struct.get $kotlin.Any___type_6 2
        struct.get $RTTI___type_4 0
        local.get $1_interfaceId
        call $kotlin.wasm.internal.wasmArrayAnyIndexOfValue___fun_37
        array.get (type $AnyArray___type_0)
        return)
    (func $kotlin.Throwable.<init>___fun_39 (type $____type_100)
        (param $0_<this> (ref null $kotlin.Throwable___type_32))
        (param $1_message (ref null $kotlin.String___type_27))
        (param $2_cause (ref null $kotlin.Throwable___type_32))
        (param $3_jsStack externref) (result (ref null $kotlin.Throwable___type_32))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_18
            ref.null none
            global.get $kotlin.Throwable_rtti___g_52
            i32.const 0
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Throwable___type_32
            ref.null extern
            ref.null $kotlin.String___type_27
            
            ref.null $kotlin.Any___type_6
            struct.new $kotlin.Throwable___type_32  ;; type: kotlin.Throwable  ;; type: kotlin.String?
            local.set $0_<this>  ;; name: message, type: kotlin.String?
        end
        local.get $0_<this>  ;; type: kotlin.Throwable  ;; type: kotlin.Throwable?
        local.get $1_message  ;; name: cause, type: kotlin.Throwable?
        struct.set $kotlin.Throwable___type_32 4
        local.get $0_<this>  ;; type: kotlin.Throwable  ;; type: kotlin.js.JsAny
        local.get $2_cause  ;; name: jsStack, type: kotlin.js.JsAny
        struct.set $kotlin.Throwable___type_32 5
        local.get $0_<this>  ;; type: kotlin.Throwable
        local.get $3_jsStack  ;; name: _stack, type: kotlin.String?
        struct.set $kotlin.Throwable___type_32 6
        local.get $0_<this>  ;; type: kotlin.Throwable
        ref.null none  ;; name: suppressedExceptionsList, type: kotlin.collections.MutableList<kotlin.Throwable>?
        struct.set $kotlin.Throwable___type_32 7
        local.get $0_<this>
        ref.null none
        struct.set $kotlin.Throwable___type_32 8
        nop
        local.get $0_<this>
        return)
    (func $kotlin.Throwable.<get-message>___fun_40 (type $____type_29)
        (param $0_<this> (ref null $kotlin.Any___type_6)) (result (ref null $kotlin.String___type_27))
        (local $1_tmp0_<this> (ref null $kotlin.Throwable___type_32))  ;; type: kotlin.Any  ;; type: kotlin.Throwable
        local.get $0_<this>  ;; name: message, type: kotlin.String?
        ref.cast null $kotlin.Throwable___type_32
        local.tee $1_tmp0_<this>
        struct.get $kotlin.Throwable___type_32 4
        return)
    (func $kotlin.Throwable.<get-jsStack>___fun_41 (type $____type_30)
        (param $0_<this> (ref null $kotlin.Any___type_6)) (result externref)
        (local $1_tmp0_<this> (ref null $kotlin.Throwable___type_32))  ;; type: kotlin.Any  ;; type: kotlin.Throwable
        local.get $0_<this>  ;; name: jsStack, type: kotlin.js.JsAny
        ref.cast null $kotlin.Throwable___type_32
        local.tee $1_tmp0_<this>
        struct.get $kotlin.Throwable___type_32 6
        return)
    (func $kotlin.Throwable.<init>___fun_42 (type $____type_101)
        (param $0_<this> (ref null $kotlin.Throwable___type_32))
        (param $1_message (ref null $kotlin.String___type_27))
        (param $2_cause (ref null $kotlin.Throwable___type_32)) (result (ref null $kotlin.Throwable___type_32))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_18
            ref.null none
            global.get $kotlin.Throwable_rtti___g_52
            i32.const 0
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Throwable___type_32
            ref.null extern
            ref.null $kotlin.String___type_27
            
            ref.null $kotlin.Any___type_6
            struct.new $kotlin.Throwable___type_32  ;; type: kotlin.String?
            local.set $0_<this>  ;; type: kotlin.Throwable?
        end
        local.get $0_<this>
        local.get $1_message
        local.get $2_cause
        call $kotlin.captureStackTrace__externalAdapter___fun_45
        call $kotlin.Throwable.<init>___fun_39
        drop
        nop
        local.get $0_<this>
        return)
    (func $kotlin.Throwable.<init>___fun_43 (type $____type_102)
        (param $0_<this> (ref null $kotlin.Throwable___type_32))
        (param $1_message (ref null $kotlin.String___type_27)) (result (ref null $kotlin.Throwable___type_32))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_18
            ref.null none
            global.get $kotlin.Throwable_rtti___g_52
            i32.const 0
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Throwable___type_32
            ref.null extern
            ref.null $kotlin.String___type_27
            
            ref.null $kotlin.Any___type_6
            struct.new $kotlin.Throwable___type_32  ;; type: kotlin.String?
            local.set $0_<this>
        end
        local.get $0_<this>
        local.get $1_message
        ref.null none
        call $kotlin.Throwable.<init>___fun_42
        drop
        nop
        local.get $0_<this>
        return)
    (func $kotlin.Throwable.<init>___fun_44 (type $____type_103)
        (param $0_<this> (ref null $kotlin.Throwable___type_32)) (result (ref null $kotlin.Throwable___type_32))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_18
            ref.null none
            global.get $kotlin.Throwable_rtti___g_52
            i32.const 0
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Throwable___type_32
            ref.null extern
            ref.null $kotlin.String___type_27
            
            ref.null $kotlin.Any___type_6
            struct.new $kotlin.Throwable___type_32
            local.set $0_<this>
        end
        local.get $0_<this>
        ref.null none
        ref.null none
        call $kotlin.Throwable.<init>___fun_42
        drop
        nop
        local.get $0_<this>
        return)
    (func $kotlin.captureStackTrace__externalAdapter___fun_45 (type $____type_104) (result externref)
        (local $0_tmp0 externref)  ;; type: kotlin.js.JsAny?
        call $kotlin.captureStackTrace___fun_0
        call $kotlin.wasm.internal.jsCheckIsNullOrUndefinedAdapter___fun_56
        local.tee $0_tmp0
        ref.is_null
        if (result externref)  ;; type: kotlin.js.JsAny?
            call $kotlin.wasm.internal.THROW_NPE___fun_33
            unreachable
        else
            local.get $0_tmp0
        end
        return)
    (func $kotlin.wasm.internal.throwAsJsException___fun_46 (type $____type_105)
        (param $0_t (ref null $kotlin.Throwable___type_32))  ;; type: kotlin.Throwable
        
        ;; Class Virtual call: kotlin.Throwable.<get-message>  ;; type: kotlin.Throwable
        local.get $0_t
        
        local.get $0_t  ;; type: kotlin.Throwable
        struct.get $kotlin.Throwable___type_32 0
        struct.get $<classVTable>___type_31 1
        call_ref (type $____type_29)  ;; type: kotlin.Throwable
        
        ;; Class Virtual call: kotlin.Throwable.<get-jsStack>
        local.get $0_t  ;; type: kotlin.Throwable
        struct.get $kotlin.Any___type_6 2
        call $kotlin.wasm.internal.getSimpleName___fun_35
        local.get $0_t
        
        local.get $0_t
        struct.get $kotlin.Throwable___type_32 0
        struct.get $<classVTable>___type_31 2
        call_ref (type $____type_30)
        call $kotlin.wasm.internal.throwJsError__externalAdapter___fun_47
        unreachable)
    (func $kotlin.wasm.internal.throwJsError__externalAdapter___fun_47 (type $____type_106)
        (param $0_message (ref null $kotlin.String___type_27))
        (param $1_wasmTypeName (ref null $kotlin.String___type_27))
        (param $2_stack externref)
        (local $3_tmp2 nullref)
        (local $4_tmp0 (ref null $kotlin.String___type_27))
        (local $5_tmp1 (ref null $kotlin.String___type_27))  ;; type: kotlin.String?  ;; type: kotlin.String?
        local.get $0_message
        local.tee $4_tmp0
        ref.is_null  ;; type: kotlin.String?
        if (result externref)
            ref.null noextern
        else  ;; type: kotlin.String?
            local.get $4_tmp0
            call $kotlin.wasm.internal.kotlinToJsStringAdapter___fun_50  ;; type: kotlin.String?
        end
        local.get $1_wasmTypeName
        local.tee $5_tmp1
        ref.is_null  ;; type: kotlin.String?
        if (result externref)
            ref.null noextern
        else  ;; type: kotlin.js.JsAny?
            local.get $5_tmp1
            call $kotlin.wasm.internal.kotlinToJsStringAdapter___fun_50
        end  ;; type: kotlin.Nothing?
        local.get $2_stack
        call $kotlin.wasm.internal.throwJsError___fun_1
        local.tee $3_tmp2
        ref.is_null
        if  ;; type: kotlin.Nothing?
            call $kotlin.wasm.internal.THROW_NPE___fun_33
            unreachable
        else
            unreachable
        end
        unreachable)
    (func $kotlin.wasm.internal.createJsException___fun_48 (type $____type_108)
        (param $0_jsError externref) (result (ref null $kotlin.js.JsException___type_36))  ;; type: kotlin.js.JsAny
        ref.null none
        local.get $0_jsError
        call $kotlin.js.JsException.<init>___fun_62
        return)
    (func $kotlin.wasm.internal.jsToKotlinStringAdapter___fun_49 (type $____type_109)
        (param $0_x externref) (result (ref null $kotlin.String___type_27))
        (local $1_stringLength i32)
        (local $2_dstArray (ref null $kotlin.wasm.internal.WasmCharArray___type_26))
        (local $3_tmp0 (ref null $kotlin.wasm.internal.WasmCharArray___type_26))
        (local $4_this (ref null $kotlin.wasm.internal.WasmCharArray___type_26))
        (local $5_allocator (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))
        (local $6_result (ref null $kotlin.Any___type_6))
        (local $7_tmp (ref null $kotlin.Any___type_6))
        (local $8_tmp0 (ref null $kotlin.wasm.unsafe.MemoryAllocator___type_66))
        (local $9_allocator (ref null $kotlin.wasm.unsafe.MemoryAllocator___type_66))
        (local $10_maxStringLength i32)
        (local $11_memBuffer i32)
        (local $12_tmp0 i32)
        (local $13_this i32)
        (local $14_srcStartIndex i32)
        (local $15_t (ref null $kotlin.Throwable___type_32))
        (local $16_tmp0 (ref null $kotlin.wasm.internal.WasmCharArray___type_26))
        (local $17_this (ref null $kotlin.wasm.internal.WasmCharArray___type_26))  ;; type: kotlin.js.JsAny  ;; type: kotlin.Int
        local.get $0_x  ;; @WasmArrayOf ctor call: kotlin.wasm.internal.WasmCharArray
        call $kotlin.wasm.internal.stringLength___fun_2  ;; type: kotlin.Int
        local.tee $1_stringLength
        array.new_default $kotlin.wasm.internal.WasmCharArray___type_26
        local.set $2_dstArray
        local.get $1_stringLength  ;; type: kotlin.wasm.internal.WasmCharArray
        i32.const 0
        i32.eq
        
        ;; Inlined call of `kotlin.createString`
        if
            local.get $2_dstArray  ;; type: kotlin.wasm.internal.WasmCharArray
            local.set $3_tmp0
            
            ;; Any parameters
            block (result (ref null $kotlin.String___type_27))
                nop
                local.get $3_tmp0
                local.set $4_this
                
                global.get $<classVTable>___g_17
                global.get $<classITable>___g_34  ;; type: kotlin.wasm.internal.WasmCharArray
                global.get $kotlin.String_rtti___g_51
                i32.const 0  ;; type: kotlin.wasm.internal.WasmCharArray
                ref.null none  ;; @WasmPrimitiveConstructor ctor call: kotlin.String
                local.get $4_this
                array.len
                
                local.get $4_this
                struct.new $kotlin.String___type_27
                br 0
                
                ;; Inlined call of `kotlin.wasm.unsafe.withScopedMemoryAllocator`
            end
            return
        end
        block (result (ref null $kotlin.Unit___type_59))
            nop
            call $kotlin.wasm.unsafe.createAllocatorInTheNewScope___fun_97
            local.set $5_allocator  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
            block (result (ref null $kotlin.Any___type_6))
                block
                    
                    ;; Inlined call of `UNKNOWN`
                    try (result (ref null $kotlin.Throwable___type_32))
                        local.get $5_allocator  ;; type: kotlin.wasm.unsafe.MemoryAllocator
                        local.set $8_tmp0
                        block (result (ref null $kotlin.Any___type_6))
                            nop
                            local.get $8_tmp0  ;; type: kotlin.wasm.unsafe.MemoryAllocator
                            local.set $9_allocator  ;; type: kotlin.Int
                            i32.const 32768  ;; type: kotlin.Int
                            local.set $10_maxStringLength
                            local.get $9_allocator
                            local.get $1_stringLength
                            
                            ;; Class Virtual call: kotlin.wasm.unsafe.MemoryAllocator.allocate
                            local.get $10_maxStringLength  ;; type: kotlin.wasm.unsafe.MemoryAllocator
                            call $kotlin.ranges.coerceAtMost___fun_13
                            i32.const 2
                            i32.mul
                            
                            local.get $9_allocator
                            struct.get $kotlin.wasm.unsafe.MemoryAllocator___type_66 0
                            struct.get $<classVTable>___type_65 1
                            
                            ;; Inlined call of `kotlin.UInt.toInt`
                            call_ref (type $____type_64)
                            call $kotlin.wasm.unsafe.<Pointer__<get-address>-impl>___fun_91  ;; type: kotlin.UInt
                            local.set $12_tmp0
                            block (result i32)  ;; type: kotlin.UInt
                                nop
                                local.get $12_tmp0
                                
                                local.tee $13_this
                                call $kotlin.<UInt__<get-data>-impl>___fun_15
                                br 0
                            end
                            local.set $11_memBuffer
                            i32.const 0  ;; type: kotlin.Int
                            local.set $14_srcStartIndex  ;; type: kotlin.Int
                            loop  ;; type: kotlin.Int
                                block
                                    local.get $14_srcStartIndex
                                    local.get $1_stringLength
                                    local.get $10_maxStringLength
                                    i32.sub  ;; type: kotlin.js.JsAny
                                    i32.lt_s  ;; type: kotlin.Int
                                    i32.eqz  ;; type: kotlin.Int
                                    br_if 0  ;; type: kotlin.Int
                                    local.get $0_x
                                    local.get $14_srcStartIndex
                                    local.get $10_maxStringLength  ;; type: kotlin.Int  ;; type: kotlin.Int
                                    local.get $11_memBuffer  ;; type: kotlin.Int
                                    call $kotlin.wasm.internal.jsExportStringToWasm___fun_3  ;; type: kotlin.wasm.internal.WasmCharArray
                                    local.get $11_memBuffer
                                    local.get $14_srcStartIndex
                                    local.get $10_maxStringLength  ;; type: kotlin.Int  ;; type: kotlin.Int
                                    local.get $2_dstArray
                                    call $kotlin.wasm.internal.unsafeRawMemoryToWasmCharArray___fun_30  ;; type: kotlin.Int
                                    local.get $14_srcStartIndex
                                    local.get $10_maxStringLength
                                    i32.add
                                    local.set $14_srcStartIndex
                                    br 1  ;; type: kotlin.js.JsAny  ;; type: kotlin.Int
                                end  ;; type: kotlin.Int
                            end  ;; type: kotlin.Int
                            local.get $0_x
                            local.get $14_srcStartIndex  ;; type: kotlin.Int
                            local.get $1_stringLength
                            local.get $14_srcStartIndex
                            i32.sub  ;; type: kotlin.Int  ;; type: kotlin.Int
                            local.get $11_memBuffer  ;; type: kotlin.Int
                            call $kotlin.wasm.internal.jsExportStringToWasm___fun_3  ;; type: kotlin.Int
                            local.get $11_memBuffer
                            local.get $14_srcStartIndex  ;; type: kotlin.wasm.internal.WasmCharArray
                            local.get $1_stringLength
                            local.get $14_srcStartIndex
                            i32.sub  ;; type: kotlin.Unit?
                            local.get $2_dstArray
                            
                            call $kotlin.wasm.internal.unsafeRawMemoryToWasmCharArray___fun_30
                            global.get $kotlin.Unit_instance___g_8
                            br 0
                        end
                        br 2
                    catch 0
                    catch_all
                        ref.null extern  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
                        call $kotlin.wasm.internal.createJsException___fun_48
                    end
                    local.set $15_t  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator  ;; name: parent, type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
                    local.get $5_allocator  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
                    call $kotlin.wasm.unsafe.ScopedMemoryAllocator.destroy___fun_96
                    local.get $5_allocator  ;; type: kotlin.Throwable
                    struct.get $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 4
                    global.set $kotlin.wasm.unsafe.currentAllocator___g_9
                    local.get $15_t
                    
                    throw 0
                end  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
                unreachable
            end
            local.set $7_tmp  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator  ;; name: parent, type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
            local.get $5_allocator  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
            call $kotlin.wasm.unsafe.ScopedMemoryAllocator.destroy___fun_96
            local.get $5_allocator  ;; type: kotlin.Any?
            struct.get $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 4  ;; type: kotlin.Any?
            global.set $kotlin.wasm.unsafe.currentAllocator___g_9
            local.get $7_tmp
            
            local.set $6_result
            global.get $kotlin.Unit_instance___g_8  ;; type: kotlin.wasm.internal.WasmCharArray
            br 0
        end
        
        ;; Inlined call of `kotlin.createString`
        drop
        local.get $2_dstArray  ;; type: kotlin.wasm.internal.WasmCharArray
        local.set $16_tmp0
        
        ;; Any parameters
        block (result (ref null $kotlin.String___type_27))
            nop
            local.get $16_tmp0
            local.set $17_this
            
            global.get $<classVTable>___g_17
            global.get $<classITable>___g_34  ;; type: kotlin.wasm.internal.WasmCharArray
            global.get $kotlin.String_rtti___g_51
            i32.const 0  ;; type: kotlin.wasm.internal.WasmCharArray
            ref.null none  ;; @WasmPrimitiveConstructor ctor call: kotlin.String
            local.get $17_this
            array.len
            
            local.get $17_this
            struct.new $kotlin.String___type_27
            br 0
        end
        return)
    (func $kotlin.wasm.internal.kotlinToJsStringAdapter___fun_50 (type $____type_112)
        (param $0_x (ref null $kotlin.String___type_27)) (result externref)
        (local $1_tmp0 (ref null $kotlin.Any___type_6))
        (local $2_this (ref null $kotlin.Any___type_6))
        (local $3_srcArray (ref null $kotlin.wasm.internal.WasmCharArray___type_26))
        (local $4_tmp0 (ref null $kotlin.String___type_27))
        (local $5_this (ref null $kotlin.String___type_27))
        (local $6_stringLength i32)
        (local $7_maxStringLength i32)
        (local $8_allocator (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))
        (local $9_result (ref null $kotlin.Any___type_6))
        (local $10_tmp (ref null $kotlin.Any___type_6))
        (local $11_tmp externref)
        (local $12_tmp0 (ref null $kotlin.wasm.unsafe.MemoryAllocator___type_66))
        (local $13_allocator (ref null $kotlin.wasm.unsafe.MemoryAllocator___type_66))
        (local $14_memBuffer i32)
        (local $15_tmp0 i32)
        (local $16_this i32)
        (local $17_result externref)
        (local $18_srcStartIndex i32)
        (local $19_t (ref null $kotlin.Throwable___type_32))  ;; type: kotlin.String?
        local.get $0_x
        ref.is_null
        if  ;; type: kotlin.String?
            ref.null noextern
            return
            
            ;; Inlined call of `kotlin.text.isEmpty`
        end
        local.get $0_x  ;; type: kotlin.CharSequence
        local.set $1_tmp0
        block (result i32)  ;; type: kotlin.CharSequence
            nop  ;; type: kotlin.CharSequence
            
            ;; Special Interface call: kotlin.CharSequence.<get-length>
            local.get $1_tmp0
            local.tee $2_this
            local.get $2_this
            struct.get $kotlin.Any___type_6 0
            struct.get $<classVTable>___type_5 0
            
            struct.get $SpecialITable___type_2 17
            ref.cast $<classITable>___type_75
            struct.get $<classITable>___type_75 0
            call_ref (type $____type_24)
            
            i32.const 0
            i32.eq
            br 0
        end
        if  ;; type: kotlin.String?
            call $kotlin.wasm.internal.<get-jsEmptyString>___fun_51
            return
            
            ;; Inlined call of `kotlin.String.chars`
        end
        local.get $0_x  ;; type: kotlin.String
        local.set $4_tmp0
        block (result (ref null $kotlin.wasm.internal.WasmCharArray___type_26))  ;; type: kotlin.String
            nop
            local.get $4_tmp0
            local.tee $5_this
            call $kotlin.access$<get-leftIfInSum>___fun_25  ;; type: kotlin.String
            ref.is_null
            i32.eqz
            if  ;; type: kotlin.Unit?  ;; type: kotlin.String
                local.get $5_this
                call $kotlin.String.foldChars___fun_28
            end
            
            local.get $5_this
            call $kotlin.access$<get-_chars>___fun_26  ;; type: kotlin.wasm.internal.WasmCharArray
            br 0
        end
        local.tee $3_srcArray
        array.len
        
        ;; Inlined call of `kotlin.wasm.unsafe.withScopedMemoryAllocator`
        local.set $6_stringLength
        i32.const 32768
        local.set $7_maxStringLength
        block
            nop
            call $kotlin.wasm.unsafe.createAllocatorInTheNewScope___fun_97
            local.set $8_allocator
            block (result (ref null $kotlin.Any___type_6))  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
                block (result externref)
                    block
                        
                        ;; Inlined call of `UNKNOWN`
                        try (result (ref null $kotlin.Throwable___type_32))
                            local.get $8_allocator  ;; type: kotlin.wasm.unsafe.MemoryAllocator
                            local.set $12_tmp0
                            block (result (ref null $kotlin.Any___type_6))  ;; type: kotlin.wasm.unsafe.MemoryAllocator
                                nop  ;; type: kotlin.Int
                                local.get $12_tmp0  ;; type: kotlin.Int
                                local.tee $13_allocator
                                local.get $6_stringLength
                                
                                ;; Class Virtual call: kotlin.wasm.unsafe.MemoryAllocator.allocate
                                local.get $7_maxStringLength  ;; type: kotlin.wasm.unsafe.MemoryAllocator
                                call $kotlin.ranges.coerceAtMost___fun_13
                                i32.const 2
                                i32.mul
                                
                                local.get $13_allocator
                                struct.get $kotlin.wasm.unsafe.MemoryAllocator___type_66 0
                                struct.get $<classVTable>___type_65 1
                                
                                ;; Inlined call of `kotlin.UInt.toInt`
                                call_ref (type $____type_64)
                                call $kotlin.wasm.unsafe.<Pointer__<get-address>-impl>___fun_91  ;; type: kotlin.UInt
                                local.set $15_tmp0
                                block (result i32)  ;; type: kotlin.UInt
                                    nop
                                    local.get $15_tmp0
                                    
                                    local.tee $16_this
                                    call $kotlin.<UInt__<get-data>-impl>___fun_15
                                    br 0
                                end
                                local.set $14_memBuffer
                                ref.null noextern
                                local.set $17_result
                                i32.const 0  ;; type: kotlin.Int
                                local.set $18_srcStartIndex  ;; type: kotlin.Int
                                loop  ;; type: kotlin.Int
                                    block
                                        local.get $18_srcStartIndex
                                        local.get $6_stringLength
                                        local.get $7_maxStringLength
                                        i32.sub  ;; type: kotlin.wasm.internal.WasmCharArray
                                        i32.lt_s  ;; type: kotlin.Int
                                        i32.eqz  ;; type: kotlin.Int
                                        br_if 0  ;; type: kotlin.Int
                                        local.get $3_srcArray
                                        local.get $18_srcStartIndex
                                        local.get $7_maxStringLength  ;; type: kotlin.Int  ;; type: kotlin.Int
                                        local.get $14_memBuffer  ;; type: kotlin.js.JsAny?
                                        call $kotlin.wasm.internal.unsafeWasmCharArrayToRawMemory___fun_31
                                        local.get $14_memBuffer  ;; type: kotlin.js.JsAny?
                                        local.get $7_maxStringLength
                                        local.get $17_result  ;; type: kotlin.Int  ;; type: kotlin.Int
                                        call $kotlin.wasm.internal.importStringFromWasm__externalAdapter___fun_52
                                        local.set $17_result  ;; type: kotlin.Int
                                        local.get $18_srcStartIndex
                                        local.get $7_maxStringLength
                                        i32.add
                                        local.set $18_srcStartIndex
                                        br 1  ;; type: kotlin.wasm.internal.WasmCharArray  ;; type: kotlin.Int
                                    end  ;; type: kotlin.Int
                                end  ;; type: kotlin.Int
                                local.get $3_srcArray
                                local.get $18_srcStartIndex  ;; type: kotlin.Int
                                local.get $6_stringLength
                                local.get $18_srcStartIndex
                                i32.sub  ;; type: kotlin.Int  ;; type: kotlin.Int
                                local.get $14_memBuffer  ;; type: kotlin.Int
                                call $kotlin.wasm.internal.unsafeWasmCharArrayToRawMemory___fun_31
                                local.get $14_memBuffer  ;; type: kotlin.js.JsAny?
                                local.get $6_stringLength
                                local.get $18_srcStartIndex
                                i32.sub
                                
                                local.get $17_result
                                call $kotlin.wasm.internal.importStringFromWasm__externalAdapter___fun_52
                                br 3
                            end
                            br 3
                        catch 0
                        catch_all
                            ref.null extern  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
                            call $kotlin.wasm.internal.createJsException___fun_48
                        end
                        local.set $19_t  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator  ;; name: parent, type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
                        local.get $8_allocator  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
                        call $kotlin.wasm.unsafe.ScopedMemoryAllocator.destroy___fun_96
                        local.get $8_allocator  ;; type: kotlin.Throwable
                        struct.get $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 4
                        global.set $kotlin.wasm.unsafe.currentAllocator___g_9
                        local.get $19_t
                        
                        throw 0
                    end  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
                    unreachable
                end
                local.set $11_tmp  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator  ;; name: parent, type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
                local.get $8_allocator  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
                call $kotlin.wasm.unsafe.ScopedMemoryAllocator.destroy___fun_96
                local.get $8_allocator  ;; type: kotlin.js.JsString?
                struct.get $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 4
                
                global.set $kotlin.wasm.unsafe.currentAllocator___g_9
                local.get $11_tmp  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
                return
            end
            local.set $10_tmp  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator  ;; name: parent, type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
            local.get $8_allocator  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
            call $kotlin.wasm.unsafe.ScopedMemoryAllocator.destroy___fun_96
            local.get $8_allocator  ;; type: kotlin.Any?
            struct.get $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 4  ;; type: kotlin.Any?
            global.set $kotlin.wasm.unsafe.currentAllocator___g_9
            
            local.get $10_tmp
            local.set $9_result
            unreachable
        end
        unreachable)
    (func $kotlin.wasm.internal.<get-jsEmptyString>___fun_51 (type $____type_104) (result externref)
        (local $0_value externref)  ;; type: kotlin.js.JsString?  ;; type: kotlin.js.JsString?
        global.get $kotlin.wasm.internal._jsEmptyString___g_5
        local.tee $0_value  ;; type: kotlin.js.JsString?
        ref.is_null
        if  ;; type: kotlin.js.JsString?  ;; type: kotlin.js.JsString?
            call $kotlin.wasm.internal.getJsEmptyString__externalAdapter___fun_53  ;; type: kotlin.Unit?  ;; type: kotlin.js.JsString?
            local.tee $0_value
            global.set $kotlin.wasm.internal._jsEmptyString___g_5
        end
        local.get $0_value
        return)
    (func $kotlin.wasm.internal.importStringFromWasm__externalAdapter___fun_52 (type $____type_113)
        (param $0_address i32)
        (param $1_length i32)
        (param $2_prefix externref) (result externref)
        (local $3_tmp0 externref)  ;; type: kotlin.Int  ;; type: kotlin.Int  ;; type: kotlin.js.JsAny?
        local.get $0_address
        local.get $1_length
        local.get $2_prefix  ;; type: kotlin.js.JsAny?
        call $kotlin.wasm.internal.importStringFromWasm___fun_4
        call $kotlin.wasm.internal.jsCheckIsNullOrUndefinedAdapter___fun_56
        local.tee $3_tmp0
        ref.is_null
        if (result externref)  ;; type: kotlin.js.JsAny?
            call $kotlin.wasm.internal.THROW_NPE___fun_33
            unreachable
        else
            local.get $3_tmp0
        end
        return)
    (func $kotlin.wasm.internal.getJsEmptyString__externalAdapter___fun_53 (type $____type_104) (result externref)
        (local $0_tmp0 externref)  ;; type: kotlin.js.JsAny?
        call $kotlin.wasm.internal.getJsEmptyString___fun_5
        call $kotlin.wasm.internal.jsCheckIsNullOrUndefinedAdapter___fun_56
        local.tee $0_tmp0
        ref.is_null
        if (result externref)  ;; type: kotlin.js.JsAny?
            call $kotlin.wasm.internal.THROW_NPE___fun_33
            unreachable
        else
            local.get $0_tmp0
        end
        return)
    (func $kotlin.wasm.internal.kotlinToJsAnyAdapter___fun_54 (type $____type_30)
        (param $0_x (ref null $kotlin.Any___type_6)) (result externref)  ;; type: kotlin.Any?
        local.get $0_x
        ref.is_null  ;; type: kotlin.Any?
        if (result externref)
            ref.null noextern
        else
            local.get $0_x
            call $kotlin.wasm.internal.anyToExternRef___fun_55
        end
        return)
    (func $kotlin.wasm.internal.anyToExternRef___fun_55 (type $____type_30)
        (param $0_x (ref null $kotlin.Any___type_6)) (result externref)  ;; type: kotlin.Any  ;; type: kotlin.Any
        local.get $0_x
        ref.test $kotlin.wasm.internal.JsExternalBox___type_34  ;; name: ref, type: kotlin.js.JsAny
        if (result externref)
            local.get $0_x  ;; type: kotlin.Any
            ref.cast null $kotlin.wasm.internal.JsExternalBox___type_34
            struct.get $kotlin.wasm.internal.JsExternalBox___type_34 4
        else
            local.get $0_x
            extern.externalize
        end
        return)
    (func $kotlin.wasm.internal.jsCheckIsNullOrUndefinedAdapter___fun_56 (type $____type_114)
        (param $0_x externref) (result externref)  ;; type: kotlin.js.JsAny?
        local.get $0_x
        call $kotlin.wasm.internal.isNullish___fun_6  ;; type: kotlin.js.JsAny?
        if (result externref)
            ref.null noextern
        else
            local.get $0_x
        end
        return)
    (func $kotlin.wasm.internal.jsToKotlinAnyAdapter___fun_57 (type $____type_115)
        (param $0_x externref) (result (ref null $kotlin.Any___type_6))  ;; type: kotlin.js.JsAny?
        local.get $0_x
        ref.is_null  ;; type: kotlin.js.JsAny?
        if (result (ref null $kotlin.Any___type_6))
            ref.null none
        else
            local.get $0_x
            call $kotlin.wasm.internal.externRefToAny___fun_58
        end
        return)
    (func $kotlin.wasm.internal.externRefToAny___fun_58 (type $____type_115)
        (param $0_ref externref) (result (ref null $kotlin.Any___type_6))
        block (result anyref)
            local.get $0_ref
            extern.internalize
            br_on_cast_fail 3 0 any $kotlin.Any___type_6
            return  ;; type: kotlin.js.JsAny
            
            ;; Any parameters
        end
        drop
        local.get $0_ref
        
        global.get $<classVTable>___g_19  ;; type: kotlin.js.JsAny
        ref.null none  ;; @WasmPrimitiveConstructor ctor call: kotlin.wasm.internal.JsExternalBox
        global.get $kotlin.wasm.internal.JsExternalBox_rtti___g_53
        i32.const 0
        local.get $0_ref
        struct.new $kotlin.wasm.internal.JsExternalBox___type_34
        extern.externalize
        call $kotlin.wasm.internal.getCachedJsObject_$external_fun__externalAdapter___fun_59
        call $kotlin.wasm.internal.jsToKotlinAnyAdapter___fun_57
        return)
    (func $kotlin.wasm.internal.getCachedJsObject_$external_fun__externalAdapter___fun_59 (type $____type_116)
        (param $0_ref externref)
        (param $1_ifNotCached externref) (result externref)  ;; type: kotlin.js.JsAny?  ;; type: kotlin.js.JsAny?
        local.get $0_ref
        local.get $1_ifNotCached
        call $kotlin.wasm.internal.getCachedJsObject_$external_fun___fun_7
        call $kotlin.wasm.internal.jsCheckIsNullOrUndefinedAdapter___fun_56
        return)
    (func $kotlin.js.<get-stackPlaceHolder>___fun_60 (type $____type_104) (result externref)  ;; type: kotlin.js.JsAny
        call $kotlin.js.<init_properties_JsException.kt>___fun_69
        global.get $kotlin.js.stackPlaceHolder___g_6
        return)
    (func $kotlin.js.stackPlaceHolder_js_code__externalAdapter___fun_61 (type $____type_104) (result externref)
        (local $0_tmp0 externref)
        call $kotlin.js.<init_properties_JsException.kt>___fun_69  ;; type: kotlin.js.JsAny?
        call $kotlin.js.stackPlaceHolder_js_code___fun_8
        call $kotlin.wasm.internal.jsCheckIsNullOrUndefinedAdapter___fun_56
        local.tee $0_tmp0
        ref.is_null
        if (result externref)  ;; type: kotlin.js.JsAny?
            call $kotlin.wasm.internal.THROW_NPE___fun_33
            unreachable
        else
            local.get $0_tmp0
        end
        return)
    (func $kotlin.js.JsException.<init>___fun_62 (type $____type_117)
        (param $0_<this> (ref null $kotlin.js.JsException___type_36))
        (param $1_thrownValue externref) (result (ref null $kotlin.js.JsException___type_36))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_20
            ref.null none
            global.get $kotlin.js.JsException_rtti___g_60
            i32.const 0
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Throwable___type_32
            ref.null extern
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Any___type_6
            ref.null extern
            ref.null $kotlin.String___type_27
            
            ref.null extern
            struct.new $kotlin.js.JsException___type_36
            local.set $0_<this>
        end
        local.get $0_<this>
        ref.null none
        ref.null none  ;; type: kotlin.js.JsException
        call $kotlin.js.<get-stackPlaceHolder>___fun_60  ;; type: kotlin.js.JsAny?
        call $kotlin.Throwable.<init>___fun_39  ;; name: thrownValue, type: kotlin.js.JsAny?
        drop
        local.get $0_<this>  ;; type: kotlin.js.JsException
        local.get $1_thrownValue  ;; name: _message, type: kotlin.String?
        struct.set $kotlin.js.JsException___type_36 9
        local.get $0_<this>  ;; type: kotlin.js.JsException
        ref.null none  ;; name: _jsStack, type: kotlin.js.JsAny?
        struct.set $kotlin.js.JsException___type_36 10
        local.get $0_<this>
        ref.null noextern
        struct.set $kotlin.js.JsException___type_36 11
        nop
        local.get $0_<this>
        return)
    (func $kotlin.js.JsException.<get-message>___fun_63 (type $____type_29)
        (param $0_<this> (ref null $kotlin.Any___type_6)) (result (ref null $kotlin.String___type_27))
        (local $1_tmp0_<this> (ref null $kotlin.js.JsException___type_36))
        (local $2_value (ref null $kotlin.String___type_27))
        (local $3_tmp externref)  ;; type: kotlin.Any  ;; type: kotlin.js.JsException
        local.get $0_<this>  ;; name: _message, type: kotlin.String?
        ref.cast null $kotlin.js.JsException___type_36  ;; type: kotlin.String?
        local.tee $1_tmp0_<this>
        struct.get $kotlin.js.JsException___type_36 10  ;; type: kotlin.js.JsException
        local.tee $2_value  ;; name: thrownValue, type: kotlin.js.JsAny?
        ref.is_null
        if  ;; type: kotlin.js.JsAny?
            local.get $1_tmp0_<this>
            struct.get $kotlin.js.JsException___type_36 9
            local.tee $3_tmp
            ref.is_null  ;; type: kotlin.js.JsAny?
            if (result i32)
                i32.const 0
            else
                local.get $3_tmp
                call $kotlin.wasm.internal.jsToKotlinAnyAdapter___fun_57  ;; type: kotlin.js.JsException
                call $kotlin.js.JsError_$external_class_instanceof__externalAdapter___fun_68  ;; name: thrownValue, type: kotlin.js.JsAny?
            end
            if (result (ref null $kotlin.String___type_27))
                
                ;; const string: "Exception was thrown while running JavaScript code"
                local.get $1_tmp0_<this>
                struct.get $kotlin.js.JsException___type_36 9
                call $kotlin.js.message_$external_prop_getter__externalAdapter___fun_66
            else
                
                i32.const 14
                i32.const 234  ;; type: kotlin.String?
                i32.const 50
                call $kotlin.stringLiteral___fun_29  ;; type: kotlin.js.JsException  ;; type: kotlin.String?
            end  ;; name: _message, type: kotlin.String?
            local.set $2_value
            local.get $1_tmp0_<this>  ;; type: kotlin.Unit?  ;; type: kotlin.String?
            local.get $2_value
            struct.set $kotlin.js.JsException___type_36 10
        end
        local.get $2_value
        return)
    (func $kotlin.js.JsException.<get-message>___fun_64 (type $____type_29)
        (param $0_<this> (ref null $kotlin.Any___type_6)) (result (ref null $kotlin.String___type_27))  ;; type: kotlin.Any
        local.get $0_<this>
        call $kotlin.js.JsException.<get-message>___fun_63
        return)
    (func $kotlin.js.JsException.<get-jsStack>___fun_65 (type $____type_30)
        (param $0_<this> (ref null $kotlin.Any___type_6)) (result externref)
        (local $1_tmp0_<this> (ref null $kotlin.js.JsException___type_36))
        (local $2_value externref)
        (local $3_tmp externref)  ;; type: kotlin.Any  ;; type: kotlin.js.JsException
        local.get $0_<this>  ;; name: _jsStack, type: kotlin.js.JsAny?
        ref.cast null $kotlin.js.JsException___type_36  ;; type: kotlin.js.JsAny?
        local.tee $1_tmp0_<this>
        struct.get $kotlin.js.JsException___type_36 11  ;; type: kotlin.js.JsException
        local.tee $2_value  ;; name: thrownValue, type: kotlin.js.JsAny?
        ref.is_null
        if  ;; type: kotlin.js.JsAny?
            local.get $1_tmp0_<this>
            struct.get $kotlin.js.JsException___type_36 9
            local.tee $3_tmp
            ref.is_null  ;; type: kotlin.js.JsAny?
            if (result i32)
                i32.const 0
            else
                local.get $3_tmp
                call $kotlin.wasm.internal.jsToKotlinAnyAdapter___fun_57  ;; type: kotlin.js.JsException
                call $kotlin.js.JsError_$external_class_instanceof__externalAdapter___fun_68  ;; name: thrownValue, type: kotlin.js.JsAny?
            end
            if (result externref)
                local.get $1_tmp0_<this>
                struct.get $kotlin.js.JsException___type_36 9
                call $kotlin.js.stack_$external_prop_getter__externalAdapter___fun_67  ;; type: kotlin.js.JsAny?
            else
                call $kotlin.js.<get-stackPlaceHolder>___fun_60  ;; type: kotlin.js.JsException  ;; type: kotlin.js.JsAny?
            end  ;; name: _jsStack, type: kotlin.js.JsAny?
            local.set $2_value
            local.get $1_tmp0_<this>  ;; type: kotlin.Unit?  ;; type: kotlin.js.JsAny?
            local.get $2_value
            struct.set $kotlin.js.JsException___type_36 11
        end
        local.get $2_value
        return)
    (func $kotlin.js.message_$external_prop_getter__externalAdapter___fun_66 (type $____type_109)
        (param $0__this externref) (result (ref null $kotlin.String___type_27))
        (local $1_tmp0 externref)  ;; type: kotlin.js.JsError
        call $kotlin.js.<init_properties_JsException.kt>___fun_69  ;; type: kotlin.js.JsAny?
        local.get $0__this
        call $kotlin.js.message_$external_prop_getter___fun_9
        local.tee $1_tmp0
        ref.is_null
        if (result (ref null $kotlin.String___type_27))  ;; type: kotlin.js.JsAny?
            call $kotlin.wasm.internal.THROW_NPE___fun_33
            unreachable
        else
            local.get $1_tmp0
            call $kotlin.wasm.internal.jsToKotlinStringAdapter___fun_49
        end
        return)
    (func $kotlin.js.stack_$external_prop_getter__externalAdapter___fun_67 (type $____type_114)
        (param $0__this externref) (result externref)
        (local $1_tmp0 externref)  ;; type: kotlin.js.JsError
        call $kotlin.js.<init_properties_JsException.kt>___fun_69
        local.get $0__this  ;; type: kotlin.js.JsAny?
        call $kotlin.js.stack_$external_prop_getter___fun_10
        call $kotlin.wasm.internal.jsCheckIsNullOrUndefinedAdapter___fun_56
        local.tee $1_tmp0
        ref.is_null
        if (result externref)  ;; type: kotlin.js.JsAny?
            call $kotlin.wasm.internal.THROW_NPE___fun_33
            unreachable
        else
            local.get $1_tmp0
        end
        return)
    (func $kotlin.js.JsError_$external_class_instanceof__externalAdapter___fun_68 (type $____type_24)
        (param $0_x (ref null $kotlin.Any___type_6)) (result i32)  ;; type: kotlin.Any
        call $kotlin.js.<init_properties_JsException.kt>___fun_69
        local.get $0_x
        call $kotlin.wasm.internal.kotlinToJsAnyAdapter___fun_54
        call $kotlin.js.JsError_$external_class_instanceof___fun_11
        return)
    (func $kotlin.js.<init_properties_JsException.kt>___fun_69 (type $____type_95)  ;; type: kotlin.Boolean
        global.get $kotlin.js.properties_initialized_JsException.kt___g_7  ;; type: kotlin.Boolean
        if
        else  ;; type: kotlin.js.JsAny
            i32.const 1
            global.set $kotlin.js.properties_initialized_JsException.kt___g_7
            call $kotlin.js.stackPlaceHolder_js_code__externalAdapter___fun_61
            global.set $kotlin.js.stackPlaceHolder___g_6
        end
        nop)
    (func $kotlin.IllegalArgumentException.<init>___fun_70 (type $____type_118)
        (param $0_<this> (ref null $kotlin.IllegalArgumentException___type_42))
        (param $1_message (ref null $kotlin.String___type_27)) (result (ref null $kotlin.IllegalArgumentException___type_42))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_21
            ref.null none
            global.get $kotlin.IllegalArgumentException_rtti___g_64
            i32.const 0
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Throwable___type_32
            ref.null extern
            ref.null $kotlin.String___type_27
            
            ref.null $kotlin.Any___type_6
            struct.new $kotlin.IllegalArgumentException___type_42  ;; type: kotlin.String?
            local.set $0_<this>
        end
        local.get $0_<this>
        local.get $1_message
        call $kotlin.RuntimeException.<init>___fun_77
        drop
        local.get $0_<this>
        call $kotlin.IllegalArgumentException.<init>___fun_71
        drop
        nop
        local.get $0_<this>
        return)
    (func $kotlin.IllegalArgumentException.<init>___fun_71 (type $____type_119)
        (param $0_<this> (ref null $kotlin.IllegalArgumentException___type_42)) (result (ref null $kotlin.IllegalArgumentException___type_42))
        nop
        local.get $0_<this>
        return)
    (func $kotlin.ArithmeticException.<init>___fun_72 (type $____type_120)
        (param $0_<this> (ref null $kotlin.ArithmeticException___type_44))
        (param $1_message (ref null $kotlin.String___type_27)) (result (ref null $kotlin.ArithmeticException___type_44))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_22
            ref.null none
            global.get $kotlin.ArithmeticException_rtti___g_65
            i32.const 0
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Throwable___type_32
            ref.null extern
            ref.null $kotlin.String___type_27
            
            ref.null $kotlin.Any___type_6
            struct.new $kotlin.ArithmeticException___type_44  ;; type: kotlin.String?
            local.set $0_<this>
        end
        local.get $0_<this>
        local.get $1_message
        call $kotlin.RuntimeException.<init>___fun_77
        drop
        local.get $0_<this>
        call $kotlin.ArithmeticException.<init>___fun_73
        drop
        nop
        local.get $0_<this>
        return)
    (func $kotlin.ArithmeticException.<init>___fun_73 (type $____type_121)
        (param $0_<this> (ref null $kotlin.ArithmeticException___type_44)) (result (ref null $kotlin.ArithmeticException___type_44))
        nop
        local.get $0_<this>
        return)
    (func $kotlin.IndexOutOfBoundsException.<init>___fun_74 (type $____type_122)
        (param $0_<this> (ref null $kotlin.IndexOutOfBoundsException___type_47)) (result (ref null $kotlin.IndexOutOfBoundsException___type_47))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_23
            ref.null none
            global.get $kotlin.IndexOutOfBoundsException_rtti___g_66
            i32.const 0
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Throwable___type_32
            ref.null extern
            ref.null $kotlin.String___type_27
            
            ref.null $kotlin.Any___type_6
            struct.new $kotlin.IndexOutOfBoundsException___type_47
            local.set $0_<this>
        end
        local.get $0_<this>
        call $kotlin.RuntimeException.<init>___fun_76
        drop
        local.get $0_<this>
        call $kotlin.IndexOutOfBoundsException.<init>___fun_75
        drop
        nop
        local.get $0_<this>
        return)
    (func $kotlin.IndexOutOfBoundsException.<init>___fun_75 (type $____type_122)
        (param $0_<this> (ref null $kotlin.IndexOutOfBoundsException___type_47)) (result (ref null $kotlin.IndexOutOfBoundsException___type_47))
        nop
        local.get $0_<this>
        return)
    (func $kotlin.RuntimeException.<init>___fun_76 (type $____type_123)
        (param $0_<this> (ref null $kotlin.RuntimeException___type_40)) (result (ref null $kotlin.RuntimeException___type_40))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_24
            ref.null none
            global.get $kotlin.RuntimeException_rtti___g_63
            i32.const 0
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Throwable___type_32
            ref.null extern
            ref.null $kotlin.String___type_27
            
            ref.null $kotlin.Any___type_6
            struct.new $kotlin.RuntimeException___type_40
            local.set $0_<this>
        end
        local.get $0_<this>
        call $kotlin.Exception.<init>___fun_79
        drop
        local.get $0_<this>
        call $kotlin.RuntimeException.<init>___fun_78
        drop
        nop
        local.get $0_<this>
        return)
    (func $kotlin.RuntimeException.<init>___fun_77 (type $____type_124)
        (param $0_<this> (ref null $kotlin.RuntimeException___type_40))
        (param $1_message (ref null $kotlin.String___type_27)) (result (ref null $kotlin.RuntimeException___type_40))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_24
            ref.null none
            global.get $kotlin.RuntimeException_rtti___g_63
            i32.const 0
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Throwable___type_32
            ref.null extern
            ref.null $kotlin.String___type_27
            
            ref.null $kotlin.Any___type_6
            struct.new $kotlin.RuntimeException___type_40  ;; type: kotlin.String?
            local.set $0_<this>
        end
        local.get $0_<this>
        local.get $1_message
        call $kotlin.Exception.<init>___fun_80
        drop
        local.get $0_<this>
        call $kotlin.RuntimeException.<init>___fun_78
        drop
        nop
        local.get $0_<this>
        return)
    (func $kotlin.RuntimeException.<init>___fun_78 (type $____type_123)
        (param $0_<this> (ref null $kotlin.RuntimeException___type_40)) (result (ref null $kotlin.RuntimeException___type_40))
        nop
        local.get $0_<this>
        return)
    (func $kotlin.Exception.<init>___fun_79 (type $____type_125)
        (param $0_<this> (ref null $kotlin.Exception___type_38)) (result (ref null $kotlin.Exception___type_38))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_25
            ref.null none
            global.get $kotlin.Exception_rtti___g_61
            i32.const 0
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Throwable___type_32
            ref.null extern
            ref.null $kotlin.String___type_27
            
            ref.null $kotlin.Any___type_6
            struct.new $kotlin.Exception___type_38
            local.set $0_<this>
        end
        local.get $0_<this>
        call $kotlin.Throwable.<init>___fun_44
        drop
        local.get $0_<this>
        call $kotlin.Exception.<init>___fun_81
        drop
        nop
        local.get $0_<this>
        return)
    (func $kotlin.Exception.<init>___fun_80 (type $____type_126)
        (param $0_<this> (ref null $kotlin.Exception___type_38))
        (param $1_message (ref null $kotlin.String___type_27)) (result (ref null $kotlin.Exception___type_38))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_25
            ref.null none
            global.get $kotlin.Exception_rtti___g_61
            i32.const 0
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Throwable___type_32
            ref.null extern
            ref.null $kotlin.String___type_27
            
            ref.null $kotlin.Any___type_6
            struct.new $kotlin.Exception___type_38  ;; type: kotlin.String?
            local.set $0_<this>
        end
        local.get $0_<this>
        local.get $1_message
        call $kotlin.Throwable.<init>___fun_43
        drop
        local.get $0_<this>
        call $kotlin.Exception.<init>___fun_81
        drop
        nop
        local.get $0_<this>
        return)
    (func $kotlin.Exception.<init>___fun_81 (type $____type_125)
        (param $0_<this> (ref null $kotlin.Exception___type_38)) (result (ref null $kotlin.Exception___type_38))
        nop
        local.get $0_<this>
        return)
    (func $kotlin.IllegalStateException.<init>___fun_82 (type $____type_127)
        (param $0_<this> (ref null $kotlin.IllegalStateException___type_50))
        (param $1_message (ref null $kotlin.String___type_27)) (result (ref null $kotlin.IllegalStateException___type_50))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_26
            ref.null none
            global.get $kotlin.IllegalStateException_rtti___g_67
            i32.const 0
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Throwable___type_32
            ref.null extern
            ref.null $kotlin.String___type_27
            
            ref.null $kotlin.Any___type_6
            struct.new $kotlin.IllegalStateException___type_50  ;; type: kotlin.String?
            local.set $0_<this>
        end
        local.get $0_<this>
        local.get $1_message
        call $kotlin.RuntimeException.<init>___fun_77
        drop
        local.get $0_<this>
        call $kotlin.IllegalStateException.<init>___fun_83
        drop
        nop
        local.get $0_<this>
        return)
    (func $kotlin.IllegalStateException.<init>___fun_83 (type $____type_128)
        (param $0_<this> (ref null $kotlin.IllegalStateException___type_50)) (result (ref null $kotlin.IllegalStateException___type_50))
        nop
        local.get $0_<this>
        return)
    (func $kotlin.ClassCastException.<init>___fun_84 (type $____type_129)
        (param $0_<this> (ref null $kotlin.ClassCastException___type_53)) (result (ref null $kotlin.ClassCastException___type_53))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_27
            ref.null none
            global.get $kotlin.ClassCastException_rtti___g_68
            i32.const 0
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Throwable___type_32
            ref.null extern
            ref.null $kotlin.String___type_27
            
            ref.null $kotlin.Any___type_6
            struct.new $kotlin.ClassCastException___type_53
            local.set $0_<this>
        end
        local.get $0_<this>
        call $kotlin.RuntimeException.<init>___fun_76
        drop
        local.get $0_<this>
        call $kotlin.ClassCastException.<init>___fun_85
        drop
        nop
        local.get $0_<this>
        return)
    (func $kotlin.ClassCastException.<init>___fun_85 (type $____type_129)
        (param $0_<this> (ref null $kotlin.ClassCastException___type_53)) (result (ref null $kotlin.ClassCastException___type_53))
        nop
        local.get $0_<this>
        return)
    (func $kotlin.NullPointerException.<init>___fun_86 (type $____type_130)
        (param $0_<this> (ref null $kotlin.NullPointerException___type_56)) (result (ref null $kotlin.NullPointerException___type_56))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_28
            ref.null none
            global.get $kotlin.NullPointerException_rtti___g_69
            i32.const 0
            ref.null $kotlin.String___type_27
            ref.null $kotlin.Throwable___type_32
            ref.null extern
            ref.null $kotlin.String___type_27
            
            ref.null $kotlin.Any___type_6
            struct.new $kotlin.NullPointerException___type_56
            local.set $0_<this>
        end
        local.get $0_<this>
        call $kotlin.RuntimeException.<init>___fun_76
        drop
        local.get $0_<this>
        call $kotlin.NullPointerException.<init>___fun_87
        drop
        nop
        local.get $0_<this>
        return)
    (func $kotlin.NullPointerException.<init>___fun_87 (type $____type_130)
        (param $0_<this> (ref null $kotlin.NullPointerException___type_56)) (result (ref null $kotlin.NullPointerException___type_56))
        nop
        local.get $0_<this>
        return)
    (func $kotlin.Unit.<init>___fun_88 (type $____type_131)
        (param $0_<this> (ref null $kotlin.Unit___type_59)) (result (ref null $kotlin.Unit___type_59))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_29
            ref.null none
            global.get $kotlin.Unit_rtti___g_54
            
            i32.const 0
            struct.new $kotlin.Unit___type_59
            local.set $0_<this>
        end
        nop
        local.get $0_<this>
        return)
    (func $kotlin.Unit_getInstance___fun_89 (type $____type_132) (result (ref null $kotlin.Unit___type_59))  ;; type: kotlin.Unit?
        global.get $kotlin.Unit_instance___g_8
        return)
    (func $kotlin.wasm.unsafe.<Pointer__<init>-impl>___fun_90 (type $____type_81)
        (param $0_address i32) (result i32)  ;; type: kotlin.UInt
        local.get $0_address
        return)
    (func $kotlin.wasm.unsafe.<Pointer__<get-address>-impl>___fun_91 (type $____type_81)
        (param $0_$this i32) (result i32)  ;; type: kotlin.wasm.unsafe.Pointer
        local.get $0_$this
        return)
    (func $kotlin.wasm.unsafe.MemoryAllocator.<init>___fun_92 (type $____type_133)
        (param $0_<this> (ref null $kotlin.wasm.unsafe.MemoryAllocator___type_66)) (result (ref null $kotlin.wasm.unsafe.MemoryAllocator___type_66))
        nop
        local.get $0_<this>
        return)
    (func $kotlin.wasm.unsafe.ScopedMemoryAllocator.<init>___fun_93 (type $____type_134)
        (param $0_<this> (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))
        (param $1_startAddress i32)
        (param $2_parent (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68)) (result (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_31
            ref.null none
            global.get $kotlin.wasm.unsafe.ScopedMemoryAllocator_rtti___g_62
            i32.const 0
            ref.null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68
            i32.const 0
            i32.const 0
            
            i32.const 0
            struct.new $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68
            local.set $0_<this>
        end  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
        local.get $0_<this>  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
        call $kotlin.wasm.unsafe.MemoryAllocator.<init>___fun_92  ;; name: parent, type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
        drop
        local.get $0_<this>  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
        local.get $2_parent  ;; name: destroyed, type: kotlin.Boolean
        struct.set $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 4
        local.get $0_<this>  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
        i32.const 0  ;; name: suspended, type: kotlin.Boolean
        struct.set $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 5
        local.get $0_<this>  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator  ;; type: kotlin.Int
        i32.const 0  ;; name: availableAddress, type: kotlin.Int
        struct.set $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 6
        local.get $0_<this>
        local.get $1_startAddress
        struct.set $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 7
        nop
        local.get $0_<this>
        return)
    (func $kotlin.wasm.unsafe.ScopedMemoryAllocator.allocate___fun_94 (type $____type_64)
        (param $0_<this> (ref null $kotlin.Any___type_6))
        (param $1_size i32) (result i32)
        (local $2_tmp0_<this> (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))
        (local $3_tmp0 i32)
        (local $4_value i32)
        (local $5_message (ref null $kotlin.String___type_27))
        (local $6_tmp0 i32)
        (local $7_value i32)
        (local $8_message (ref null $kotlin.String___type_27))
        (local $9_align i32)
        (local $10_result i32)
        (local $11_tmp0 i32)
        (local $12_this i32)
        (local $13_tmp0 i32)
        (local $14_value i32)
        (local $15_message (ref null $kotlin.String___type_27))
        (local $16_tmp0 (ref null $kotlin.String___type_27))
        (local $17_message (ref null $kotlin.String___type_27))
        (local $18_currentMaxSize i32)
        (local $19_numPagesToGrow i32)
        (local $20_tmp0 (ref null $kotlin.String___type_27))
        (local $21_message (ref null $kotlin.String___type_27))
        (local $22_tmp0 i32)
        (local $23_value i32)
        (local $24_tmp0 i32)
        (local $25_this i32)  ;; type: kotlin.Any  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
        local.get $0_<this>  ;; name: destroyed, type: kotlin.Boolean
        ref.cast null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68
        local.tee $2_tmp0_<this>
        
        ;; Inlined call of `kotlin.check`
        struct.get_s $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 5
        i32.eqz  ;; type: kotlin.Boolean
        local.set $3_tmp0
        block (result (ref null $kotlin.Unit___type_59))  ;; type: kotlin.Boolean
            nop
            local.get $3_tmp0
            
            ;; Inlined call of `UNKNOWN`
            local.tee $4_value
            
            ;; const string: "ScopedMemoryAllocator is destroyed when out of scope"
            i32.eqz
            if
                block (result (ref null $kotlin.String___type_27))
                    nop
                    
                    i32.const 27
                    i32.const 732
                    
                    i32.const 52
                    call $kotlin.stringLiteral___fun_29
                    br 0  ;; type: kotlin.String
                end
                local.set $5_message
                ref.null none
                local.get $5_message  ;; type: kotlin.Unit?
                call $kotlin.IllegalStateException.<init>___fun_82
                throw 0
                
            end
            global.get $kotlin.Unit_instance___g_8  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
            br 0  ;; name: suspended, type: kotlin.Boolean
        end
        drop
        local.get $2_tmp0_<this>
        
        ;; Inlined call of `kotlin.check`
        struct.get_s $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 6
        i32.eqz  ;; type: kotlin.Boolean
        local.set $6_tmp0
        block (result (ref null $kotlin.Unit___type_59))  ;; type: kotlin.Boolean
            nop
            local.get $6_tmp0
            
            ;; Inlined call of `UNKNOWN`
            local.tee $7_value
            
            ;; const string: "ScopedMemoryAllocator is suspended when nested allocators are used"
            i32.eqz
            if
                block (result (ref null $kotlin.String___type_27))
                    nop
                    
                    i32.const 28
                    i32.const 836
                    
                    i32.const 66
                    call $kotlin.stringLiteral___fun_29
                    br 0  ;; type: kotlin.String
                end
                local.set $8_message
                ref.null none
                local.get $8_message  ;; type: kotlin.Unit?
                call $kotlin.IllegalStateException.<init>___fun_82
                throw 0
                
            end
            global.get $kotlin.Unit_instance___g_8
            br 0
        end  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
        drop  ;; name: availableAddress, type: kotlin.Int
        i32.const 8  ;; type: kotlin.Int
        local.set $9_align
        local.get $2_tmp0_<this>
        struct.get $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 7
        local.get $9_align  ;; type: kotlin.Int
        i32.add
        i32.const 1
        i32.sub
        local.get $9_align
        
        ;; Inlined call of `kotlin.Int.inv`
        i32.const 1
        i32.sub  ;; type: kotlin.Int
        local.set $11_tmp0
        block (result i32)  ;; type: kotlin.Int
            nop
            local.get $11_tmp0
            local.tee $12_this
            
            i32.const -1
            i32.xor
            br 0  ;; type: kotlin.Int
        end
        i32.and
        local.tee $10_result  ;; type: kotlin.Int
        i32.const 0  ;; type: kotlin.Int
        i32.ge_s
        if (result i32)
            local.get $10_result
            local.get $9_align
            call $kotlin.Int__rem-impl___fun_22
            i32.const 0
            i32.eq
        else
            
            ;; Inlined call of `kotlin.check`
            i32.const 0
        end  ;; type: kotlin.Boolean
        local.set $13_tmp0
        block (result (ref null $kotlin.Unit___type_59))  ;; type: kotlin.Boolean
            nop
            local.get $13_tmp0
            
            ;; Inlined call of `UNKNOWN`
            local.tee $14_value
            
            ;; const string: "result must be >= 0 and 8-byte aligned"
            i32.eqz
            if
                block (result (ref null $kotlin.String___type_27))
                    nop
                    
                    i32.const 29
                    i32.const 968
                    
                    i32.const 38
                    call $kotlin.stringLiteral___fun_29
                    br 0  ;; type: kotlin.String
                end
                local.set $15_message
                ref.null none
                local.get $15_message  ;; type: kotlin.Unit?
                call $kotlin.IllegalStateException.<init>___fun_82
                throw 0
                
            end
            global.get $kotlin.Unit_instance___g_8
            br 0  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
        end  ;; name: availableAddress, type: kotlin.Int
        drop
        i32.const 2147483647  ;; type: kotlin.Int
        local.get $2_tmp0_<this>
        struct.get $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 7
        
        ;; const string: "Out of linear memory. All available address space (2gb) is used."
        i32.sub
        local.get $1_size
        i32.lt_s
        if
            
            i32.const 30
            i32.const 1044
            
            ;; Inlined call of `kotlin.error`
            i32.const 64
            call $kotlin.stringLiteral___fun_29  ;; type: kotlin.String
            local.set $16_tmp0
            block
                nop  ;; type: kotlin.String
                local.get $16_tmp0
                local.set $17_message
                ref.null none
                
                local.get $17_message
                call $kotlin.IllegalStateException.<init>___fun_82
                throw 0  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
            end  ;; type: kotlin.Int
            unreachable  ;; type: kotlin.Int
        end
        local.get $2_tmp0_<this>  ;; name: availableAddress, type: kotlin.Int
        local.get $10_result
        local.get $1_size
        i32.add
        struct.set $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 7
        memory.size  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
        i32.const 65536  ;; name: availableAddress, type: kotlin.Int
        i32.mul  ;; type: kotlin.Int
        local.set $18_currentMaxSize
        local.get $2_tmp0_<this>
        struct.get $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 7  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
        local.get $18_currentMaxSize  ;; name: availableAddress, type: kotlin.Int
        i32.ge_s  ;; type: kotlin.Int
        if
            local.get $2_tmp0_<this>
            struct.get $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 7
            local.get $18_currentMaxSize
            i32.sub
            i32.const 65536
            call $kotlin.Int__div-impl___fun_21  ;; type: kotlin.Int
            i32.const 2
            i32.add
            local.tee $19_numPagesToGrow
            
            ;; const string: "Out of linear memory. memory.grow returned -1"
            memory.grow
            i32.const -1
            i32.eq
            if
                
                i32.const 31
                i32.const 1172
                
                ;; Inlined call of `kotlin.error`
                i32.const 45
                call $kotlin.stringLiteral___fun_29  ;; type: kotlin.String
                local.set $20_tmp0
                block
                    nop  ;; type: kotlin.String
                    local.get $20_tmp0
                    local.set $21_message
                    ref.null none
                    
                    local.get $21_message
                    call $kotlin.IllegalStateException.<init>___fun_82
                    throw 0
                end  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
                unreachable  ;; name: availableAddress, type: kotlin.Int
            end
        end
        local.get $2_tmp0_<this>
        struct.get $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 7
        memory.size
        i32.const 65536
        
        ;; Inlined call of `kotlin.check`
        i32.mul
        i32.lt_s  ;; type: kotlin.Boolean
        local.set $22_tmp0
        block (result (ref null $kotlin.Unit___type_59))  ;; type: kotlin.Boolean
            nop
            local.get $22_tmp0
            
            ;; const string: "Check failed."
            local.tee $23_value
            i32.eqz
            if
                ref.null none
                
                i32.const 10
                i32.const 142
                i32.const 13
                call $kotlin.stringLiteral___fun_29  ;; type: kotlin.Unit?
                call $kotlin.IllegalStateException.<init>___fun_82
                throw 0
                
            end
            global.get $kotlin.Unit_instance___g_8  ;; type: kotlin.Int
            br 0
        end
        
        ;; Inlined call of `kotlin.toUInt`
        drop
        local.get $10_result  ;; type: kotlin.Int
        local.set $24_tmp0
        block (result i32)  ;; type: kotlin.Int
            nop
            local.get $24_tmp0
            
            local.tee $25_this
            call $kotlin.<UInt__<init>-impl>___fun_14
            br 0
        end
        call $kotlin.wasm.unsafe.<Pointer__<init>-impl>___fun_90
        return)
    (func $kotlin.wasm.unsafe.ScopedMemoryAllocator.createChild___fun_95 (type $____type_135)
        (param $0_<this> (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68)) (result (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))
        (local $1_child (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))
        (local $2_tmp0 i32)
        (local $3_this i32)  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator  ;; name: availableAddress, type: kotlin.Int
        ref.null none
        
        ;; Inlined call of `kotlin.Int.toInt`
        local.get $0_<this>
        struct.get $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 7  ;; type: kotlin.Int
        local.set $2_tmp0
        block (result i32)  ;; type: kotlin.Int
            nop
            local.get $2_tmp0
              ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
            local.tee $3_this
            br 0
        end  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
        local.get $0_<this>
        call $kotlin.wasm.unsafe.ScopedMemoryAllocator.<init>___fun_93  ;; name: suspended, type: kotlin.Boolean
        local.set $1_child
        local.get $0_<this>  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
        i32.const 1
        struct.set $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 6
        local.get $1_child
        return)
    (func $kotlin.wasm.unsafe.ScopedMemoryAllocator.destroy___fun_96 (type $____type_136)
        (param $0_<this> (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))
        (local $1_tmp0_safe_receiver (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator  ;; name: destroyed, type: kotlin.Boolean
        local.get $0_<this>  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator  ;; name: parent, type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
        i32.const 1
        struct.set $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 5  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
        local.get $0_<this>
        struct.get $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 4
        local.tee $1_tmp0_safe_receiver  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
        ref.is_null
        if  ;; name: suspended, type: kotlin.Boolean
        else
            local.get $1_tmp0_safe_receiver  ;; type: kotlin.Unit?
            i32.const 0
            struct.set $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68 6
        end
        nop)
    (func $kotlin.wasm.unsafe.createAllocatorInTheNewScope___fun_97 (type $____type_137) (result (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))
        (local $0_allocator (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))
        (local $1_tmp1_elvis_lhs (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))
        (local $2_tmp0_safe_receiver (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator?  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
        global.get $kotlin.wasm.unsafe.currentAllocator___g_9
        local.tee $2_tmp0_safe_receiver
        ref.is_null  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
        if (result (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))
            ref.null none
        else
            local.get $2_tmp0_safe_receiver  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
            call $kotlin.wasm.unsafe.ScopedMemoryAllocator.createChild___fun_95
        end
        local.tee $1_tmp1_elvis_lhs
        ref.is_null
        if (result (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))
            ref.null none
            i32.const 0  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
            ref.null none
            call $kotlin.wasm.unsafe.ScopedMemoryAllocator.<init>___fun_93
        else  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
            local.get $1_tmp1_elvis_lhs  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator?
        end  ;; type: kotlin.wasm.unsafe.ScopedMemoryAllocator
        local.tee $0_allocator
        global.set $kotlin.wasm.unsafe.currentAllocator___g_9
        local.get $0_allocator
        return)
    (func $<get-x>___fun_98 (type $____type_138) (result (ref null $kotlin.Any___type_6))  ;; type: kotlin.Function1<kotlin.Int, kotlin.Int>
        call $<init_properties_equalsHashCode.kt>___fun_109
        global.get $x___g_35
        return)
    (func $<get-y>___fun_99 (type $____type_138) (result (ref null $kotlin.Any___type_6))  ;; type: kotlin.Function1<kotlin.Int, kotlin.Int>
        call $<init_properties_equalsHashCode.kt>___fun_109
        global.get $y___g_36
        return)
    (func $lolkek___fun_100 (type $____type_139)
        (param $0_x i32)
        (param $1_f (ref null $kotlin.Any___type_6)) (result i32)  ;; type: kotlin.Function1<kotlin.Int, kotlin.Int>
        
        ;; Any parameters
        call $<init_properties_equalsHashCode.kt>___fun_109
        local.get $1_f
        
        global.get $<classVTable>___g_15  ;; type: kotlin.Int
        global.get $<classITable>___g_33  ;; box
        global.get $kotlin.Int_rtti___g_59  ;; type: kotlin.Function1<kotlin.Int, kotlin.Int>
        
        ;; Functional Interface call: kotlin.Function1.invoke
        i32.const 0
        local.get $0_x
        struct.new $kotlin.Int___type_20
        local.get $1_f
        struct.get $kotlin.Any___type_6 0
        struct.get $<classVTable>___type_5 0
        struct.get $SpecialITable___type_2 22
        
        i32.const 1
        array.get (type $FuncArray___type_1)  ;; name: value, type: kotlin.Int
        ref.cast $____type_69
        call_ref (type $____type_69)
        ref.cast $kotlin.Int___type_20
        struct.get $kotlin.Int___type_20 4
        i32.const 20
        i32.add
        return)
    (func $box___fun_101 (type $____type_140) (result (ref null $kotlin.String___type_27))
        (local $0_inductionVariable i32)
        (local $1_i i32)  ;; type: kotlin.Int
        call $<init_properties_equalsHashCode.kt>___fun_109
        i32.const 0
        local.tee $0_inductionVariable
        i32.const 100000
        i32.le_s
        if  ;; type: kotlin.Int
            loop
                block  ;; type: kotlin.Int
                    block
                        local.get $0_inductionVariable
                        local.set $1_i  ;; type: kotlin.Int
                        local.get $0_inductionVariable
                        i32.const 1  ;; type: kotlin.Int
                        i32.add
                        local.set $0_inductionVariable
                        local.get $1_i
                        i32.const 2
                        call $kotlin.Int__rem-impl___fun_22  ;; type: kotlin.Int
                        i32.const 0
                        i32.eq
                        if
                            local.get $1_i
                            call $<get-x>___fun_98  ;; type: kotlin.Int
                            call $lolkek___fun_100
                            drop
                        else
                            local.get $1_i
                            call $<get-y>___fun_99
                            call $lolkek___fun_100  ;; type: kotlin.Int
                            drop
                        end
                    end
                    local.get $0_inductionVariable
                    i32.const 100000
                    i32.le_s
                    br_if 1  ;; type: kotlin.Unit?
                    
                    ;; const string: "OK"
                end
            end
        end
        
        i32.const 32
        i32.const 1262
        i32.const 2
        call $kotlin.stringLiteral___fun_29
        return)
    (func $box__JsExportAdapter___fun_102 (type $____type_104) (result externref)
        (local $0_currentIsNotFirstWasmExportCall i32)
        (local $1_tmp externref)
        (local $2_e (ref null $kotlin.Throwable___type_32))
        (local $3_t (ref null $kotlin.Throwable___type_32))  ;; type: kotlin.Boolean
        global.get $kotlin.wasm.internal.isNotFirstWasmExportCall___g_3
        local.set $0_currentIsNotFirstWasmExportCall
        block (result (ref null $kotlin.Unit___type_59))
            block (result externref)
                block
                    try (result (ref null $kotlin.Throwable___type_32))  ;; type: kotlin.Boolean
                        block
                            try (result (ref null $kotlin.Throwable___type_32))
                                i32.const 1
                                global.set $kotlin.wasm.internal.isNotFirstWasmExportCall___g_3
                                call $box___fun_101
                                call $kotlin.wasm.internal.kotlinToJsStringAdapter___fun_50
                                br 4
                            catch 0
                            catch_all
                                ref.null extern  ;; type: kotlin.Boolean
                                call $kotlin.wasm.internal.createJsException___fun_48
                            end  ;; type: kotlin.Throwable
                            local.set $2_e
                            local.get $0_currentIsNotFirstWasmExportCall
                            if (result (ref null $kotlin.Unit___type_59))  ;; type: kotlin.Throwable
                                local.get $2_e
                                throw 0
                            else
                                local.get $2_e
                                call $kotlin.wasm.internal.throwAsJsException___fun_46
                                unreachable
                            end
                            br 4
                        end
                        unreachable
                    catch 0
                    catch_all
                        ref.null extern  ;; type: kotlin.Boolean
                        call $kotlin.wasm.internal.createJsException___fun_48  ;; type: kotlin.Boolean
                    end
                    local.set $3_t  ;; type: kotlin.Throwable
                    local.get $0_currentIsNotFirstWasmExportCall
                    global.set $kotlin.wasm.internal.isNotFirstWasmExportCall___g_3
                    local.get $3_t
                    
                    throw 0
                end  ;; type: kotlin.Boolean
                unreachable  ;; type: kotlin.Boolean
            end
            local.set $1_tmp  ;; type: kotlin.js.JsString?
            local.get $0_currentIsNotFirstWasmExportCall
            
            global.set $kotlin.wasm.internal.isNotFirstWasmExportCall___g_3
            local.get $1_tmp  ;; type: kotlin.Boolean
            return  ;; type: kotlin.Boolean
        end
        drop
        local.get $0_currentIsNotFirstWasmExportCall
        global.set $kotlin.wasm.internal.isNotFirstWasmExportCall___g_3
        unreachable)
    (func $x$lambda.<init>___fun_103 (type $____type_141)
        (param $0_<this> (ref null $x$lambda___type_71)) (result (ref null $x$lambda___type_71))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_40
            global.get $<classITable>___g_42
            global.get $x$lambda_rtti___g_57
            
            i32.const 0
            struct.new $x$lambda___type_71
            local.set $0_<this>
        end
        nop
        local.get $0_<this>
        return)
    (func $x$lambda.invoke___fun_104 (type $____type_64)
        (param $0_<this> (ref null $kotlin.Any___type_6))
        (param $1_it i32) (result i32)  ;; type: kotlin.Int
        local.get $1_it
        i32.const 42
        i32.add
        return)
    (func $x$lambda.invoke___fun_105 (type $____type_69)
        (param $0_<this> (ref null $kotlin.Any___type_6))
        (param $1_p1 (ref null $kotlin.Any___type_6)) (result (ref null $kotlin.Any___type_6))
        
        ;; Any parameters
        
        global.get $<classVTable>___g_15  ;; type: kotlin.Function1<P1 of kotlin.Function1, R of kotlin.Function1>
        global.get $<classITable>___g_33  ;; type: P1 of kotlin.Function1
        global.get $kotlin.Int_rtti___g_59
        i32.const 0
        local.get $0_<this>
        local.get $1_p1
        ref.is_null  ;; type: P1 of kotlin.Function1
        if (result i32)
            i32.const 0
        else
            local.get $1_p1  ;; type: P1 of kotlin.Function1
            ref.test $kotlin.Int___type_20
        end  ;; name: value, type: kotlin.Int
        if (result i32)
            local.get $1_p1
            ref.cast $kotlin.Int___type_20
            struct.get $kotlin.Int___type_20 4
        else
            call $kotlin.wasm.internal.THROW_CCE___fun_34  ;; box
            unreachable
        end
        call $x$lambda.invoke___fun_104
        struct.new $kotlin.Int___type_20
        return)
    (func $y$lambda.<init>___fun_106 (type $____type_142)
        (param $0_<this> (ref null $y$lambda___type_73)) (result (ref null $y$lambda___type_73))
        
        ;; Object creation prefix
        
        ;; Any parameters
        local.get $0_<this>
        ref.is_null
        if
            
            global.get $<classVTable>___g_41
            global.get $<classITable>___g_43
            global.get $y$lambda_rtti___g_58
            
            i32.const 0
            struct.new $y$lambda___type_73
            local.set $0_<this>
        end
        nop
        local.get $0_<this>
        return)
    (func $y$lambda.invoke___fun_107 (type $____type_64)
        (param $0_<this> (ref null $kotlin.Any___type_6))
        (param $1_it i32) (result i32)  ;; type: kotlin.Int
        local.get $1_it
        i32.const 24
        i32.add
        return)
    (func $y$lambda.invoke___fun_108 (type $____type_69)
        (param $0_<this> (ref null $kotlin.Any___type_6))
        (param $1_p1 (ref null $kotlin.Any___type_6)) (result (ref null $kotlin.Any___type_6))
        
        ;; Any parameters
        
        global.get $<classVTable>___g_15  ;; type: kotlin.Function1<P1 of kotlin.Function1, R of kotlin.Function1>
        global.get $<classITable>___g_33  ;; type: P1 of kotlin.Function1
        global.get $kotlin.Int_rtti___g_59
        i32.const 0
        local.get $0_<this>
        local.get $1_p1
        ref.is_null  ;; type: P1 of kotlin.Function1
        if (result i32)
            i32.const 0
        else
            local.get $1_p1  ;; type: P1 of kotlin.Function1
            ref.test $kotlin.Int___type_20
        end  ;; name: value, type: kotlin.Int
        if (result i32)
            local.get $1_p1
            ref.cast $kotlin.Int___type_20
            struct.get $kotlin.Int___type_20 4
        else
            call $kotlin.wasm.internal.THROW_CCE___fun_34  ;; box
            unreachable
        end
        call $y$lambda.invoke___fun_107
        struct.new $kotlin.Int___type_20
        return)
    (func $<init_properties_equalsHashCode.kt>___fun_109 (type $____type_95)  ;; type: kotlin.Boolean
        global.get $properties_initialized_equalsHashCode.kt___g_39  ;; type: kotlin.Boolean
        if
        else  ;; type: <root>.x$lambda?  ;; type: kotlin.Function1<kotlin.Int, kotlin.Int>
            i32.const 1
            global.set $properties_initialized_equalsHashCode.kt___g_39  ;; type: <root>.y$lambda?  ;; type: kotlin.Function1<kotlin.Int, kotlin.Int>
            global.get $x$lambda_instance___g_37
            global.set $x___g_35
            global.get $y$lambda_instance___g_38
            global.set $y___g_36
        end
        nop)
    (func $_fieldInitialize___fun_110 (type $____type_143)
        ref.null none
        i32.const 35
        call $kotlin.Array.<init>___fun_17
        global.set $kotlin.wasm.internal.stringPool___g_4
        ref.null none
        call $y$lambda.<init>___fun_106
        global.set $y$lambda_instance___g_38
        ref.null none
        call $x$lambda.<init>___fun_103
        global.set $x$lambda_instance___g_37
        ref.null none
        call $kotlin.Unit.<init>___fun_88
        global.set $kotlin.Unit_instance___g_8
        ref.null none
        call $kotlin.Companion.<init>___fun_24
        global.set $kotlin.Companion_instance___g_2
        ref.null none
        call $kotlin.Companion.<init>___fun_20
        global.set $kotlin.Companion_instance___g_1
        ref.null none
        call $kotlin.Companion.<init>___fun_16
        global.set $kotlin.Companion_instance___g_0)
    (func $_initialize___fun_111 (type $____type_143)
        call $kotlin.Unit_getInstance___fun_89
        call $_fieldInitialize___fun_110
        return)
    (memory $____mem_0 0)
    (global $kotlin.Companion_instance___g_0 (mut (ref null $kotlin.Companion___type_10))
        ref.null $kotlin.Companion___type_10)
    (global $kotlin.Companion_instance___g_1 (mut (ref null $kotlin.Companion___type_17))
        ref.null $kotlin.Companion___type_17)
    (global $kotlin.Companion_instance___g_2 (mut (ref null $kotlin.Companion___type_22))
        ref.null $kotlin.Companion___type_22)
    (global $kotlin.wasm.internal.isNotFirstWasmExportCall___g_3 (mut i32)
        i32.const 0)
    (global $kotlin.wasm.internal.stringPool___g_4 (mut (ref null $kotlin.Array___type_15))
        ref.null $kotlin.Array___type_15)
    (global $kotlin.wasm.internal._jsEmptyString___g_5 (mut externref)
        ref.null noextern)
    (global $kotlin.js.stackPlaceHolder___g_6 (mut externref)
        ref.null extern)
    (global $kotlin.js.properties_initialized_JsException.kt___g_7 (mut i32)
        i32.const 0)
    (global $kotlin.Unit_instance___g_8 (mut (ref null $kotlin.Unit___type_59))
        ref.null $kotlin.Unit___type_59)
    (global $kotlin.wasm.unsafe.currentAllocator___g_9 (mut (ref null $kotlin.wasm.unsafe.ScopedMemoryAllocator___type_68))
        ref.null none)
    (global $<classVTable>___g_10 (ref $<classVTable>___type_9)
        ref.null none
        struct.new $<classVTable>___type_9)
    (global $<classVTable>___g_11 (ref $<classVTable>___type_11)
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        struct.new $<classITable>___type_76
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        struct.new $SpecialITable___type_2
        struct.new $<classVTable>___type_11)
    (global $<classVTable>___g_12 (ref $<classVTable>___type_5)
        ref.null none
        struct.new $<classVTable>___type_5)
    (global $<classVTable>___g_13 (ref $<classVTable>___type_13)
        ref.null none
        struct.new $<classVTable>___type_13)
    (global $<classVTable>___g_14 (ref $<classVTable>___type_16)
        ref.null none
        struct.new $<classVTable>___type_16)
    (global $<classVTable>___g_15 (ref $<classVTable>___type_19)
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        struct.new $<classITable>___type_76
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        struct.new $SpecialITable___type_2
        struct.new $<classVTable>___type_19)
    (global $<classVTable>___g_16 (ref $<classVTable>___type_21)
        ref.null none
        struct.new $<classVTable>___type_21)
    (global $<classVTable>___g_17 (ref $<classVTable>___type_25)
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        struct.new $<classITable>___type_76
        ref.func $kotlin.String.<get-length>___fun_27
        struct.new $<classITable>___type_75
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        struct.new $SpecialITable___type_2
        ref.func $kotlin.String.<get-length>___fun_27
        struct.new $<classVTable>___type_25)
    (global $<classVTable>___g_18 (ref $<classVTable>___type_31)
        ref.null none
        ref.func $kotlin.Throwable.<get-message>___fun_40
        ref.func $kotlin.Throwable.<get-jsStack>___fun_41
        struct.new $<classVTable>___type_31)
    (global $<classVTable>___g_19 (ref $<classVTable>___type_33)
        ref.null none
        struct.new $<classVTable>___type_33)
    (global $<classVTable>___g_20 (ref $<classVTable>___type_35)
        ref.null none
        ref.func $kotlin.js.JsException.<get-message>___fun_64
        ref.func $kotlin.js.JsException.<get-jsStack>___fun_65
        ref.func $kotlin.js.JsException.<get-message>___fun_63
        struct.new $<classVTable>___type_35)
    (global $<classVTable>___g_21 (ref $<classVTable>___type_41)
        ref.null none
        ref.func $kotlin.Throwable.<get-message>___fun_40
        ref.func $kotlin.Throwable.<get-jsStack>___fun_41
        struct.new $<classVTable>___type_41)
    (global $<classVTable>___g_22 (ref $<classVTable>___type_43)
        ref.null none
        ref.func $kotlin.Throwable.<get-message>___fun_40
        ref.func $kotlin.Throwable.<get-jsStack>___fun_41
        struct.new $<classVTable>___type_43)
    (global $<classVTable>___g_23 (ref $<classVTable>___type_46)
        ref.null none
        ref.func $kotlin.Throwable.<get-message>___fun_40
        ref.func $kotlin.Throwable.<get-jsStack>___fun_41
        struct.new $<classVTable>___type_46)
    (global $<classVTable>___g_24 (ref $<classVTable>___type_39)
        ref.null none
        ref.func $kotlin.Throwable.<get-message>___fun_40
        ref.func $kotlin.Throwable.<get-jsStack>___fun_41
        struct.new $<classVTable>___type_39)
    (global $<classVTable>___g_25 (ref $<classVTable>___type_37)
        ref.null none
        ref.func $kotlin.Throwable.<get-message>___fun_40
        ref.func $kotlin.Throwable.<get-jsStack>___fun_41
        struct.new $<classVTable>___type_37)
    (global $<classVTable>___g_26 (ref $<classVTable>___type_49)
        ref.null none
        ref.func $kotlin.Throwable.<get-message>___fun_40
        ref.func $kotlin.Throwable.<get-jsStack>___fun_41
        struct.new $<classVTable>___type_49)
    (global $<classVTable>___g_27 (ref $<classVTable>___type_52)
        ref.null none
        ref.func $kotlin.Throwable.<get-message>___fun_40
        ref.func $kotlin.Throwable.<get-jsStack>___fun_41
        struct.new $<classVTable>___type_52)
    (global $<classVTable>___g_28 (ref $<classVTable>___type_55)
        ref.null none
        ref.func $kotlin.Throwable.<get-message>___fun_40
        ref.func $kotlin.Throwable.<get-jsStack>___fun_41
        struct.new $<classVTable>___type_55)
    (global $<classVTable>___g_29 (ref $<classVTable>___type_58)
        ref.null none
        struct.new $<classVTable>___type_58)
    (global $<classVTable>___g_30 (ref $<classVTable>___type_61)
        ref.null none
        struct.new $<classVTable>___type_61)
    (global $<classVTable>___g_31 (ref $<classVTable>___type_67)
        ref.null none
        ref.func $kotlin.wasm.unsafe.ScopedMemoryAllocator.allocate___fun_94
        struct.new $<classVTable>___type_67)
    (global $<classITable>___g_32 (ref $AnyArray___type_0)
        array.new_fixed $AnyArray___type_0 0)
    (global $<classITable>___g_33 (ref $AnyArray___type_0)
        array.new_fixed $AnyArray___type_0 0)
    (global $<classITable>___g_34 (ref $AnyArray___type_0)
        array.new_fixed $AnyArray___type_0 0)
    (global $x___g_35 (mut (ref null $kotlin.Any___type_6))
        ref.null $kotlin.Any___type_6)
    (global $y___g_36 (mut (ref null $kotlin.Any___type_6))
        ref.null $kotlin.Any___type_6)
    (global $x$lambda_instance___g_37 (mut (ref null $x$lambda___type_71))
        ref.null $x$lambda___type_71)
    (global $y$lambda_instance___g_38 (mut (ref null $y$lambda___type_73))
        ref.null $y$lambda___type_73)
    (global $properties_initialized_equalsHashCode.kt___g_39 (mut i32)
        i32.const 0)
    (global $<classVTable>___g_40 (ref $<classVTable>___type_70)
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null func
        ref.func $x$lambda.invoke___fun_105
        array.new_fixed $FuncArray___type_1 2
        struct.new $SpecialITable___type_2
        ref.func $x$lambda.invoke___fun_104
        ref.func $x$lambda.invoke___fun_105
        struct.new $<classVTable>___type_70)
    (global $<classVTable>___g_41 (ref $<classVTable>___type_72)
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null none
        ref.null func
        ref.func $y$lambda.invoke___fun_108
        array.new_fixed $FuncArray___type_1 2
        struct.new $SpecialITable___type_2
        ref.func $y$lambda.invoke___fun_107
        ref.func $y$lambda.invoke___fun_108
        struct.new $<classVTable>___type_72)
    (global $<classITable>___g_42 (ref $AnyArray___type_0)
        array.new_fixed $AnyArray___type_0 0)
    (global $<classITable>___g_43 (ref $AnyArray___type_0)
        array.new_fixed $AnyArray___type_0 0)
    (global $kotlin.Any_rtti___g_44 (ref $RTTI___type_4)
        ref.null none
        ref.null none
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 38
        i32.const 3
        i32.const 4
        i64.const 2798839509968575897
        struct.new $RTTI___type_4)
    (global $kotlin.Number_rtti___g_45 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.Any_rtti___g_44
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 6
        i32.const 1
        i64.const -8592449170174531932
        struct.new $RTTI___type_4)
    (global $kotlin.Companion_rtti___g_46 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.Any_rtti___g_44
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 12
        i32.const 9
        i32.const 2
        i64.const -6607763668320844268
        struct.new $RTTI___type_4)
    (global $kotlin.UInt_rtti___g_47 (ref $RTTI___type_4)
        i64.const 5954910402192883452
        array.new_fixed $kotlin.wasm.internal.WasmLongImmutableArray___type_28 1
        global.get $kotlin.Any_rtti___g_44
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 30
        i32.const 4
        i32.const 3
        i64.const 7083758452498518870
        struct.new $RTTI___type_4)
    (global $kotlin.Array_rtti___g_48 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.Any_rtti___g_44
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 44
        i32.const 5
        i32.const 5
        i64.const -8244439916257877247
        struct.new $RTTI___type_4)
    (global $kotlin.Companion_rtti___g_49 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.Any_rtti___g_44
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 12
        i32.const 9
        i32.const 2
        i64.const 7147592030385101646
        struct.new $RTTI___type_4)
    (global $kotlin.Companion_rtti___g_50 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.Any_rtti___g_44
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 12
        i32.const 9
        i32.const 2
        i64.const 4903249906481983065
        struct.new $RTTI___type_4)
    (global $kotlin.String_rtti___g_51 (ref $RTTI___type_4)
        i64.const 5954910402192883452
        i64.const 6370907013483330859
        array.new_fixed $kotlin.wasm.internal.WasmLongImmutableArray___type_28 2
        global.get $kotlin.Any_rtti___g_44
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 130
        i32.const 6
        i32.const 9
        i64.const -6333374602768427243
        struct.new $RTTI___type_4)
    (global $kotlin.Throwable_rtti___g_52 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.Any_rtti___g_44
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 168
        i32.const 9
        i32.const 11
        i64.const -6652194674597942208
        struct.new $RTTI___type_4)
    (global $kotlin.wasm.internal.JsExternalBox_rtti___g_53 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.Any_rtti___g_44
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 186
        i32.const 13
        i32.const 12
        i64.const -5257734225098662143
        struct.new $RTTI___type_4)
    (global $kotlin.Unit_rtti___g_54 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.Any_rtti___g_44
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 638
        i32.const 4
        i32.const 23
        i64.const 2747285998620027492
        struct.new $RTTI___type_4)
    (global $kotlin.wasm.unsafe.Pointer_rtti___g_55 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.Any_rtti___g_44
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 646
        i32.const 7
        i32.const 24
        i64.const -4487840357310797298
        struct.new $RTTI___type_4)
    (global $kotlin.wasm.unsafe.MemoryAllocator_rtti___g_56 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.Any_rtti___g_44
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 660
        i32.const 15
        i32.const 25
        i64.const 1654642677112532623
        struct.new $RTTI___type_4)
    (global $x$lambda_rtti___g_57 (ref $RTTI___type_4)
        i64.const 4865718197115112024
        i64.const 7218332242951333468
        array.new_fixed $kotlin.wasm.internal.WasmLongImmutableArray___type_28 2
        global.get $kotlin.Any_rtti___g_44
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 1266
        i32.const 8
        i32.const 33
        i64.const 2144703159167525795
        struct.new $RTTI___type_4)
    (global $y$lambda_rtti___g_58 (ref $RTTI___type_4)
        i64.const 4865718197115112024
        i64.const 7218332242951333468
        array.new_fixed $kotlin.wasm.internal.WasmLongImmutableArray___type_28 2
        global.get $kotlin.Any_rtti___g_44
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 1282
        i32.const 8
        i32.const 34
        i64.const 7523473610768890175
        struct.new $RTTI___type_4)
    (global $kotlin.Int_rtti___g_59 (ref $RTTI___type_4)
        i64.const 5954910402192883452
        array.new_fixed $kotlin.wasm.internal.WasmLongImmutableArray___type_28 1
        global.get $kotlin.Number_rtti___g_45
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 124
        i32.const 3
        i32.const 8
        i64.const 1189077720114019965
        struct.new $RTTI___type_4)
    (global $kotlin.js.JsException_rtti___g_60 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.Throwable_rtti___g_52
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 212
        i32.const 11
        i32.const 13
        i64.const 8788567566637533234
        struct.new $RTTI___type_4)
    (global $kotlin.Exception_rtti___g_61 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.Throwable_rtti___g_52
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 502
        i32.const 9
        i32.const 19
        i64.const 1817363542377701804
        struct.new $RTTI___type_4)
    (global $kotlin.wasm.unsafe.ScopedMemoryAllocator_rtti___g_62 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.wasm.unsafe.MemoryAllocator_rtti___g_56
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 690
        i32.const 21
        i32.const 26
        i64.const -3385990925803172230
        struct.new $RTTI___type_4)
    (global $kotlin.RuntimeException_rtti___g_63 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.Exception_rtti___g_61
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 470
        i32.const 16
        i32.const 18
        i64.const 8306083845140529227
        struct.new $RTTI___type_4)
    (global $kotlin.IllegalArgumentException_rtti___g_64 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.RuntimeException_rtti___g_63
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 334
        i32.const 24
        i32.const 15
        i64.const -7549230862210269749
        struct.new $RTTI___type_4)
    (global $kotlin.ArithmeticException_rtti___g_65 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.RuntimeException_rtti___g_63
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 382
        i32.const 19
        i32.const 16
        i64.const 5839956794142491588
        struct.new $RTTI___type_4)
    (global $kotlin.IndexOutOfBoundsException_rtti___g_66 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.RuntimeException_rtti___g_63
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 420
        i32.const 25
        i32.const 17
        i64.const 8218141595550395280
        struct.new $RTTI___type_4)
    (global $kotlin.IllegalStateException_rtti___g_67 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.RuntimeException_rtti___g_63
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 520
        i32.const 21
        i32.const 20
        i64.const 2722369505687500671
        struct.new $RTTI___type_4)
    (global $kotlin.ClassCastException_rtti___g_68 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.RuntimeException_rtti___g_63
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 562
        i32.const 18
        i32.const 21
        i64.const 171180164838119449
        struct.new $RTTI___type_4)
    (global $kotlin.NullPointerException_rtti___g_69 (ref $RTTI___type_4)
        ref.null none
        global.get $kotlin.RuntimeException_rtti___g_63
        i32.const 0
        i32.const 0
        i32.const 0
        i32.const 598
        i32.const 20
        i32.const 22
        i64.const -1226160245839178222
        struct.new $RTTI___type_4)
    (export "box" (func $box__JsExportAdapter___fun_102))
    (export "memory" (memory $____mem_0))
    (export "_initialize" (func $_initialize___fun_111))
    (data "\4e\00\75\00\6d\00\62\00\65\00\72\00\43\00\6f\00\6d\00\70\00\61\00\6e\00\69\00\6f\00\6e\00\55\00\49\00\6e\00\74\00\41\00\6e\00\79\00\41\00\72\00\72\00\61\00\79\00\4e\00\65\00\67\00\61\00\74\00\69\00\76\00\65\00\20\00\61\00\72\00\72\00\61\00\79\00\20\00\73\00\69\00\7a\00\65\00\44\00\69\00\76\00\69\00\73\00\69\00\6f\00\6e\00\20\00\62\00\79\00\20\00\7a\00\65\00\72\00\6f\00\49\00\6e\00\74\00\53\00\74\00\72\00\69\00\6e\00\67\00\43\00\68\00\65\00\63\00\6b\00\20\00\66\00\61\00\69\00\6c\00\65\00\64\00\2e\00\54\00\68\00\72\00\6f\00\77\00\61\00\62\00\6c\00\65\00\4a\00\73\00\45\00\78\00\74\00\65\00\72\00\6e\00\61\00\6c\00\42\00\6f\00\78\00\4a\00\73\00\45\00\78\00\63\00\65\00\70\00\74\00\69\00\6f\00\6e\00\45\00\78\00\63\00\65\00\70\00\74\00\69\00\6f\00\6e\00\20\00\77\00\61\00\73\00\20\00\74\00\68\00\72\00\6f\00\77\00\6e\00\20\00\77\00\68\00\69\00\6c\00\65\00\20\00\72\00\75\00\6e\00\6e\00\69\00\6e\00\67\00\20\00\4a\00\61\00\76\00\61\00\53\00\63\00\72\00\69\00\70\00\74\00\20\00\63\00\6f\00\64\00\65\00\49\00\6c\00\6c\00\65\00\67\00\61\00\6c\00\41\00\72\00\67\00\75\00\6d\00\65\00\6e\00\74\00\45\00\78\00\63\00\65\00\70\00\74\00\69\00\6f\00\6e\00\41\00\72\00\69\00\74\00\68\00\6d\00\65\00\74\00\69\00\63\00\45\00\78\00\63\00\65\00\70\00\74\00\69\00\6f\00\6e\00\49\00\6e\00\64\00\65\00\78\00\4f\00\75\00\74\00\4f\00\66\00\42\00\6f\00\75\00\6e\00\64\00\73\00\45\00\78\00\63\00\65\00\70\00\74\00\69\00\6f\00\6e\00\52\00\75\00\6e\00\74\00\69\00\6d\00\65\00\45\00\78\00\63\00\65\00\70\00\74\00\69\00\6f\00\6e\00\45\00\78\00\63\00\65\00\70\00\74\00\69\00\6f\00\6e\00\49\00\6c\00\6c\00\65\00\67\00\61\00\6c\00\53\00\74\00\61\00\74\00\65\00\45\00\78\00\63\00\65\00\70\00\74\00\69\00\6f\00\6e\00\43\00\6c\00\61\00\73\00\73\00\43\00\61\00\73\00\74\00\45\00\78\00\63\00\65\00\70\00\74\00\69\00\6f\00\6e\00\4e\00\75\00\6c\00\6c\00\50\00\6f\00\69\00\6e\00\74\00\65\00\72\00\45\00\78\00\63\00\65\00\70\00\74\00\69\00\6f\00\6e\00\55\00\6e\00\69\00\74\00\50\00\6f\00\69\00\6e\00\74\00\65\00\72\00\4d\00\65\00\6d\00\6f\00\72\00\79\00\41\00\6c\00\6c\00\6f\00\63\00\61\00\74\00\6f\00\72\00\53\00\63\00\6f\00\70\00\65\00\64\00\4d\00\65\00\6d\00\6f\00\72\00\79\00\41\00\6c\00\6c\00\6f\00\63\00\61\00\74\00\6f\00\72\00\53\00\63\00\6f\00\70\00\65\00\64\00\4d\00\65\00\6d\00\6f\00\72\00\79\00\41\00\6c\00\6c\00\6f\00\63\00\61\00\74\00\6f\00\72\00\20\00\69\00\73\00\20\00\64\00\65\00\73\00\74\00\72\00\6f\00\79\00\65\00\64\00\20\00\77\00\68\00\65\00\6e\00\20\00\6f\00\75\00\74\00\20\00\6f\00\66\00\20\00\73\00\63\00\6f\00\70\00\65\00\53\00\63\00\6f\00\70\00\65\00\64\00\4d\00\65\00\6d\00\6f\00\72\00\79\00\41\00\6c\00\6c\00\6f\00\63\00\61\00\74\00\6f\00\72\00\20\00\69\00\73\00\20\00\73\00\75\00\73\00\70\00\65\00\6e\00\64\00\65\00\64\00\20\00\77\00\68\00\65\00\6e\00\20\00\6e\00\65\00\73\00\74\00\65\00\64\00\20\00\61\00\6c\00\6c\00\6f\00\63\00\61\00\74\00\6f\00\72\00\73\00\20\00\61\00\72\00\65\00\20\00\75\00\73\00\65\00\64\00\72\00\65\00\73\00\75\00\6c\00\74\00\20\00\6d\00\75\00\73\00\74\00\20\00\62\00\65\00\20\00\3e\00\3d\00\20\00\30\00\20\00\61\00\6e\00\64\00\20\00\38\00\2d\00\62\00\79\00\74\00\65\00\20\00\61\00\6c\00\69\00\67\00\6e\00\65\00\64\00\4f\00\75\00\74\00\20\00\6f\00\66\00\20\00\6c\00\69\00\6e\00\65\00\61\00\72\00\20\00\6d\00\65\00\6d\00\6f\00\72\00\79\00\2e\00\20\00\41\00\6c\00\6c\00\20\00\61\00\76\00\61\00\69\00\6c\00\61\00\62\00\6c\00\65\00\20\00\61\00\64\00\64\00\72\00\65\00\73\00\73\00\20\00\73\00\70\00\61\00\63\00\65\00\20\00\28\00\32\00\67\00\62\00\29\00\20\00\69\00\73\00\20\00\75\00\73\00\65\00\64\00\2e\00\4f\00\75\00\74\00\20\00\6f\00\66\00\20\00\6c\00\69\00\6e\00\65\00\61\00\72\00\20\00\6d\00\65\00\6d\00\6f\00\72\00\79\00\2e\00\20\00\6d\00\65\00\6d\00\6f\00\72\00\79\00\2e\00\67\00\72\00\6f\00\77\00\20\00\72\00\65\00\74\00\75\00\72\00\6e\00\65\00\64\00\20\00\2d\00\31\00\4f\00\4b\00\78\00\24\00\6c\00\61\00\6d\00\62\00\64\00\61\00\79\00\24\00\6c\00\61\00\6d\00\62\00\64\00\61\00")
    (tag $____tag_0 (param (ref null $kotlin.Throwable___type_32))))
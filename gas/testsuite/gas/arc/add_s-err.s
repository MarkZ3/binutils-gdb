;; Test ARC EM Code denisty ADD_S extensions.  They are only valid for
;; ARCv2 architecture.
;;
; { dg-do assemble { target arc*-*-* } }
; { dg-options "--mcpu=arc700" }
        ;; The following insns are accepted by ARCv2 only
        add_s r4,r4,-1          ; { dg-error "Error: inappropriate arguments for opcode 'add_s'" }
        add_s 0,0xAAAA5555,-1   ; { dg-error "Error: inappropriate arguments for opcode 'add_s'" }
        add_s r0,r15,0x20       ; { dg-error "Error: inappropriate arguments for opcode 'add_s'" }
        add_s r1,r15,0x20       ; { dg-error "Error: inappropriate arguments for opcode 'add_s'" }

; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt < %s -reassociate -S | FileCheck %s

; Don't produce an instruction that is a no-op because the constant is an identity constant.

define i32 @add_0(i32 %x) {
; CHECK-LABEL: @add_0(
; CHECK-NEXT:    ret i32 [[X:%.*]]
;
  %a1 = add i32 %x, -30
  %a2 = add i32 %a1, 30
  ret i32 %a2
}

define i32 @mul_1(i32 %x) {
; CHECK-LABEL: @mul_1(
; CHECK-NEXT:    ret i32 [[X:%.*]]
;
  %a1 = mul i32 %x, -1
  %a2 = mul i32 %a1, -1
  ret i32 %a2
}

define i8 @and_neg1(i8 %x) {
; CHECK-LABEL: @and_neg1(
; CHECK-NEXT:    ret i8 [[X:%.*]]
;
  %a1 = and i8 %x, 255
  %a2 = and i8 %a1, 255
  ret i8 %a2
}

define i8 @or_0(i8 %x) {
; CHECK-LABEL: @or_0(
; CHECK-NEXT:    ret i8 [[X:%.*]]
;
  %a1 = or i8 %x, 0
  %a2 = or i8 %a1, 0
  ret i8 %a2
}

define i8 @xor_0(i8 %x) {
; CHECK-LABEL: @xor_0(
; CHECK-NEXT:    ret i8 [[X:%.*]]
;
  %a1 = xor i8 %x, 42
  %a2 = xor i8 %a1, 42
  ret i8 %a2
}

; FIXME

define float @fadd_0(float %x) {
; CHECK-LABEL: @fadd_0(
; CHECK-NEXT:    [[A2:%.*]] = fadd fast float [[X:%.*]], 0.000000e+00
; CHECK-NEXT:    ret float [[A2]]
;
  %a1 = fadd fast float %x, -30.0
  %a2 = fadd fast float %a1, 30.0
  ret float %a2
}

; FIXME

define float @fmul_1(float %x) {
; CHECK-LABEL: @fmul_1(
; CHECK-NEXT:    [[A2:%.*]] = fmul fast float [[X:%.*]], 1.000000e+00
; CHECK-NEXT:    ret float [[A2]]
;
  %a1 = fmul fast float %x, 4.0
  %a2 = fmul fast float %a1, 0.25
  ret float %a2
}


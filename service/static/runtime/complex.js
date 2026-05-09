"use strict";

import { __appendStdout } from './io.js';

let MEMORY = null;

export function __bindMemory(mem) {
  MEMORY = mem;
}

function view() {
  if (!MEMORY) {
    throw new Error("complex runtime called before __bindMemory");
  }
  return new DataView(MEMORY.buffer);
}

export function complex_abs(re, im) {
  return Math.hypot(Number(re), Number(im));
}

export function complex_arg(re, im) {
  return Math.atan2(Number(im), Number(re));
}

export function complex_print(re, im) {
  re = Number(re);
  im = Number(im);
  let s = String(re);
  s += im >= 0 ? '+' : '-';
  s += String(Math.abs(im)) + 'i';
  __appendStdout(s);
}

/*
* Copyright 2023 fxdaemon.com
* Released under the MIT License
*/

const talibModule = require('./dist/talib.js');

const FuncUnstId = {
  NONE: -1,
  ADX: 0,
  ADXR: 1,
  ATR: 2,
  CMO: 3,
  DX: 4,
  EMA: 5,
  HT_DCPERIOD: 6,
  HT_DCPHASE: 7,
  HT_PHASOR: 8,
  HT_SINE: 9,
  HT_TRENDLINE: 10,
  HT_TRENDMODE: 11,
  IMI: 12,
  KAMA: 13,
  MAMA: 14,
  MFI: 15,
  MINUS_DI: 16,
  MINUS_DM: 17,
  NATR: 18,
  PLUS_DI: 19,
  PLUS_DM: 20,
  RSI: 21,
  STOCHRSI: 22,
  T3: 23,
  ALL: 24,
};

let Module;
const funcDefsCache = {};

function stringToUTF8(str = '') {
  const len = Module.lengthBytesUTF8(str) + 1;
  const heap = Module._malloc(len);
  Module.stringToUTF8(str, heap, len);
  return heap;
}

function UTF8ToString(ptr = 0) {
  if (!ptr) return '';
  const str = Module.UTF8ToString(ptr);
  Module._release(ptr);
  return str;
}

function newInt32Array({ src = [], iniSize = 0, iniValue = 0 } = {}) {
  const len = src.length || iniSize;
  if (!len) return 0;
  const heap = Module._malloc(4 * len);
  for (let i = 0; i < len; i++) {
    Module.HEAP32[(heap >> 2) + i] = src[i] || iniValue;
  }
  return heap;
}

function newFloat64Array({ src = [], iniSize = 0, iniValue = 0 } = {}) {
  const len = src.length || iniSize;
  if (!len) return 0;
  const heap = Module._malloc(8 * len);
  for (let i = 0; i < len; i++) {
    Module.HEAPF64[(heap >> 3) + i] = src[i] || iniValue;
  }
  return heap;
}

function retrieveFromInt32Array(heap, from, to) {
  const data = new Array(to - from);
  for (let i = from; i < to; i++) {
    data[i - from] = Module.HEAP32[(heap >> 2) + i];
  }
  return data;
}

function retrieveFromFloat64Array(heap, from, to) {
  const data = new Array(to - from);
  for (let i = from; i < to; i++) {
    data[i - from] = Module.HEAPF64[(heap >> 3) + i];
  }
  return data;
}

function handleError(error) {
  if (error) {
    const { id, message } = error;
    return new Error(`[${id}] ${message}`);
  }
  return new Error('Unknown error');
}

function handleRequiredError(paramName) {
  throw new Error(`${paramName} is required.`);
}

function handleInvalidError(paramName) {
  throw new Error(`${paramName} is invalid.`);
}

async function load() {
  Module = await talibModule();
}

function funcList() {
  return UTF8ToString(Module._FuncList());
}

function funcDef(name) {
  const nameUTF8 = stringToUTF8(name);
  const def = UTF8ToString(Module._FuncDef(nameUTF8));
  Module._free(nameUTF8);
  return def;
}

/**
 *
 * @param {*} inputsDef
 *            [{ name:inReal, type:real }]
 *            [{ name:inReal0, type:real }, { name:inReal1, type:real }]
 *            [{ name:inPriceOHLC, type:price, flags:[open, high, low, close] }]
 * @param {*} params
 *            { inReal:[] }
 *            { inReal0:[], inReal1:[] }
 *            { open:[], high:[], low:[], close:[] }
 * @returns
 *            { price:_, real:inReal, integer:_ }
 *            { price:_, real:[...inReal0, ...inReal1], integer:_ }
 *            { price:{open, high, low, close}, real:_, integer:_ }
 */
function handleInputsDef(inputsDef, params) {
  const price = {
    step: 0,
    data: { open: 0, high: 0, low: 0, close: 0, volume: 0, openInterest: 0 },
  };
  const real = { step: 0, data: 0 };
  const integer = { step: 0, data: 0 };

  const setData = (ri, param) => {
    if (ri.data) {
      ri.data = [...ri.data, ...param];
    } else {
      ri.data = param;
    }
  };

  inputsDef.forEach(inputDef => {
    const { type } = inputDef;
    if (type === 'price') {
      inputDef.flags.forEach(flag => {
        const param = params[flag];
        if (param === undefined) {
          handleRequiredError(flag);
        }
        if (Array.isArray(param)) {
          price.step = Math.min(param.length, price.step || param.length);
          price.data[flag] = param;
        }
      });
    } else {
      const param = params[inputDef.name];
      if (param === undefined) {
        handleRequiredError(inputDef.name);
      }
      if (Array.isArray(param)) {
        if (type === 'real') {
          setData(real, param);
        } else if (type === 'integer') {
          setData(integer, param);
        }
      }
    }
  });

  if (price.step) {
    for (let flag in price.data) {
      price.data[flag] = newFloat64Array({ src: price.data[flag] || [] });
    }
  }
  if (real.data) {
    real.step = real.data.length / inputsDef.length;
    real.data = newFloat64Array({ src: real.data });
  }
  if (integer.data) {
    integer.step = integer.data.length / inputsDef.length;
    integer.data = newFloat64Array({ src: integer.data });
  }

  return { price, real, integer };
}

/**
 *
 * @param {*} optInputsDef
 *           [{ name:optInFastPeriod, type:integer, range },
 *            { name:optInFastMAType, type:integer, list },
 *            { name:optInSlowPeriod, type:integer, range },
 *            { name:optInSlowMAType, type:integer, list },
 *            { name:optInSignalPeriod, type:integer, range },
 *            { name:optInSignalMAType, type:integer, list}]
 * @param {*} params
 *          [optInFastPeriod:12, optInFastMAType:"SMA",
 *           optInSlowPeriod:6, optInSlowMAType:"EMA",
 *           optInSignalPeriod:9, optInSignalMAType:"WMA"]
 * @returns
 *          [12, 0, 6, 1, 9, 2]
 */
function handleOptInputsDef(optInputsDef, params) {
  const optParams = new Array(optInputsDef.length).fill(0);
  optInputsDef.forEach((optInputDef, i) => {
    const { name, range, list } = optInputDef;
    const param = params[name];
    if (param === undefined) {
      handleRequiredError(name);
    }
    if (range) {
      optParams[i] = param;
    } else if (list) {
      const value = list[param];
      if (value === undefined) {
        handleInvalidError(param);
      }
      optParams[i] = value;
    }
  });
  return optParams;
}

/**
 *
 * @param {*} outputsDef
 *            [{ name:outMACD, type:real },
 *             { name:outMACDSignal, type:real },
 *             { name:outMACDHist, type:real }]
 *            [{ name:outInteger, type:integer }]
 * @returns
 *            { outReal:[outMACD, outMACDSignal, outMACDHist], outInteger:_ }
 *            { outReal:_, outInteger:[outInteger] }
 */
function handleOutputsDef(outputsDef, step) {
  let realCount = 0;
  let integerCount = 0;
  outputsDef.forEach(outputDef => {
    const { type } = outputDef;
    if (type === 'real') {
      realCount += 1;
    } else if (type === 'integer') {
      integerCount += 1;
    }
  });

  const outReal = newFloat64Array({ iniSize: step * realCount });
  const outInteger = newInt32Array({ iniSize: step * integerCount });
  return { outReal, outInteger };
}

function TA(name, params = {}) {
  let funcDefs = funcDefsCache[name];
  if (!funcDefs) {
    funcDefs = JSON.parse(funcDef(name));
    if (funcDefs.error) {
      return { error: handleError(funcDefs.error) };
    }
    funcDefsCache[name] = funcDefs;
  }

  const ret = {};
  try {
    const {
      inputs: inputsDef,
      opt_inputs: optInputsDef,
      outputs: outputsDef,
    } = funcDefs;
    const optInputsList = handleOptInputsDef(optInputsDef, params);
    const { price, real, integer } = handleInputsDef(inputsDef, params);
    const step = price.step || real.step || integer.step;
    const { outReal, outInteger } = handleOutputsDef(outputsDef, step);

    const nameUTF8 = stringToUTF8(name);
    const { open, high, low, close, volume, openInterest } = price.data;
    const inReal = real.data;
    const inInteger = integer.data;
    const optInputs = newFloat64Array({ src: optInputsList });

    let outRange = 0;
    let retPtr = 0;
    const { startIdx, endIdx } = params;
    if (startIdx >= 0 && endIdx > 0) {
      outRange = newInt32Array({ iniSize: 2 });
      retPtr = Module._TA(
        nameUTF8,
        startIdx,
        endIdx,
        step,
        open,
        high,
        low,
        close,
        volume,
        openInterest,
        inReal,
        inInteger,
        optInputs,
        outReal,
        outInteger,
        outRange,
      );
    } else {
      retPtr = Module._TAL(
        nameUTF8,
        step,
        open,
        high,
        low,
        close,
        volume,
        openInterest,
        inReal,
        inInteger,
        optInputs,
        outReal,
        outInteger,
      );
    }

    if (retPtr) {
      const retObj = JSON.parse(UTF8ToString(retPtr));
      ret.error = handleError(retObj.error);
    } else {
      if (outReal) {
        outputsDef
          .filter(outputDef => outputDef.type === 'real')
          .forEach((outputDef, i) => {
            ret[outputDef.name] = retrieveFromFloat64Array(
              outReal,
              i * step,
              (i + 1) * step,
            );
          });
      }
      if (outInteger) {
        outputsDef
          .filter(outputDef => outputDef.type === 'integer')
          .forEach((outputDef, i) => {
            ret[outputDef.name] = retrieveFromInt32Array(
              outInteger,
              i * step,
              (i + 1) * step,
            );
          });
      }
      if (outRange) {
        const [outBegIdx, outNBElement] = retrieveFromInt32Array(
          outRange,
          0,
          2,
        );
        ret.outBegIdx = outBegIdx;
        ret.outNBElement = outNBElement;
      }
    }

    Module._free(nameUTF8);
    open && Module._free(open);
    high && Module._free(high);
    low && Module._free(low);
    close && Module._free(close);
    volume && Module._free(volume);
    openInterest && Module._free(openInterest);
    inReal && Module._free(inReal);
    inInteger && Module._free(inInteger);
    optInputs && Module._free(optInputs);
    outReal && Module._free(outReal);
    outInteger && Module._free(outInteger);
    outRange && Module._free(outRange);
  } catch (e) {
    ret.error = e;
  }
  return ret;
}

function setUnstablePeriod(funcId, unstablePeriod = 0) {
  const retPtr = Module._SetUnstablePeriod(
    FuncUnstId[funcId] || FuncUnstId.NONE,
    unstablePeriod,
  );
  if (retPtr) {
    return UTF8ToString(retPtr);
  }
}

module.exports = { load, funcList, funcDef, TA, setUnstablePeriod };

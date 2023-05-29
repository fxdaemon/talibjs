# talibjs
Port the TA-Lib C library to WebAssembly, including all indicators from the original TA-Lib.

## Installation

```bash
npm install talibjs
```

## Usage

```javascript
const talib = require('@fxdaemon/talibjs');
// Or: import talib from '@fxdaemon/talibjs';

async function run() {
  await talib.load();
  // Call the API here.
}

run();
```

- get a list of functions

```javascript
const retFuncList = talib.funcList();
console.log(retFuncList);
```
```bash
["ADD","DIV","MAX","MAXINDEX","MIN","MININDEX", ...]
```

- get function definition in json format

```javascript
const retFuncDef = talib.funcDef("SMA");
console.log(retFuncDef);
```
```bash
{"name":"SMA","group":"Overlap Studies","hint":"Simple Moving Average","inputs":[{"name":"inReal","type":"real"}],"opt_inputs":[{"name":"optInTimePeriod","display_name":"Time Period","default_value":30.0,"hint":"Number of period","type":"integer","range":{"min":2,"max":100000,"suggested_start":4,"suggested_end":200,"suggested_increment":1}}],"outputs":[{"name":"outReal","type":"real","flags":["line"]}]}
```

- calculate an indicator
```javascript
const inReal = getPrices();
const {
  error,
  outReal,
  outBegIdx,
  outNBElement,
} = talib.TA('SMA', {
  startIdx: 0,
  endIdx: inReal.length - 1,
  inReal,
  optInTimePeriod: 14,
});
if (error) {
  console.log(error.message);
} else {
  console.log(outReal);
}
```

- set unstable period
```javascript
talib.setUnstablePeriod(funcId, 14);
```
\# available values for funcId:
```
"ADX","ADXR","ATR","CMO","DX","EMA","HT_DCPERIOD","HT_DCPHASE","HT_PHASOR","HT_SINE","HT_TRENDLINE","HT_TRENDMODE","IMI","KAMA","MAMA","MFI","MINUS_DI","MINUS_DM","NATR","PLUS_DI","PLUS_DM","RSI","STOCHRSI","T3","ALL"
```

## Building
```bash
emcc -O3 -sWASM=1 -sALLOW_MEMORY_GROWTH=1 -sMODULARIZE -sEXPORTED_FUNCTIONS=_malloc,_free,_main -sEXPORTED_RUNTIME_METHODS=UTF8ToString,stringToUTF8,lengthBytesUTF8 -I ../include talib.c yyjson.c ../lib/libta_lib.a -o ../dist/talib.js
```

## License
Talibjs  is licensed under a [MIT License](./LICENSE).

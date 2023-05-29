/*
* Copyright 2023 fxdaemon.com
* Released under the MIT License
*/

#include <stdarg.h>
#include <emscripten.h>
#include "ta_libc.h"
#include "yyjson.h"

char *handleError(const char *id, const char *format, ...)
{
  char buf[1024];
  va_list args;
  va_start(args, format);
  vsprintf(buf, format, args);
  va_end(args);

  yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  yyjson_mut_val *error = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_str(doc, error, "id", id);
  yyjson_mut_obj_add_str(doc, error, "message", buf);
  yyjson_mut_obj_add_val(doc, root, "error", error);
  
  char *json = yyjson_mut_write(doc, 0, NULL);
  yyjson_mut_doc_free(doc);
  return json;
}

char *handleErrorCode(TA_RetCode retCode)
{
  TA_RetCodeInfo retCodeInfo;
  TA_SetRetCodeInfo(retCode, &retCodeInfo);
  return handleError(retCodeInfo.enumStr, retCodeInfo.infoStr);
}

char *handleRequiredError(const char *paramName)
{
  return handleError("TA_FUNC_ERR_ARGUMENT", "[%s] is required.", paramName);
}

void shiftReal(double outReal[], int from, int outBegIdx, int outNBElement) {
  for (int i = outNBElement - 1; i >= 0 ; i--) {
    outReal[from + outBegIdx + i] = outReal[from + i];
  }
  for (int i = 0; i < outBegIdx ; i++) {
    outReal[from + i] = 0;
  }
}

void shiftInteger(int outInteger[], int from, int outBegIdx, int outNBElement) {
  for (int i = outNBElement - 1; i >= 0 ; i--) {
    outInteger[from + outBegIdx + i] = outInteger[from + i];
  }
  for (int i = 0; i < outBegIdx ; i++) {
    outInteger[from + i] = 0;
  }
}

char *preTA(const char *funcName, const TA_FuncHandle **funcHandle, const TA_FuncInfo **funcInfo)
{
  TA_RetCode retCode;
  if ((retCode = TA_GetFuncHandle(funcName, funcHandle)) != TA_SUCCESS) {
    return handleErrorCode(retCode);
  }
  if ((retCode = TA_GetFuncInfo(*funcHandle, funcInfo)) != TA_SUCCESS) {
    return handleErrorCode(retCode);
  }
  return NULL;
}

#define HANDLE_REQUIRED_ERROR(paramName) \
  TA_ParamHolderFree(funcParams); \
  return handleRequiredError(paramName);

#define HANDLE_ERROR_CODE(retCode) \
  TA_ParamHolderFree(funcParams); \
  return handleErrorCode(retCode);

char *ta(const TA_FuncHandle *funcHandle, const TA_FuncInfo *funcInfo,
         const char *funcName, int startIdx, int endIdx, int step,
         double open[], double high[], double low[], double close[],
         double volume[], double openInterest[],
         double inReal[], int inInteger[], double optParams[],
         double outReal[], int outInteger[], int outRange[])
{
  TA_RetCode retCode;
  TA_ParamHolder *funcParams;
  if ((retCode = TA_ParamHolderAlloc(funcHandle, &funcParams)) != TA_SUCCESS) {
    return handleErrorCode(retCode);
  }

  for (unsigned int i = 0; i < funcInfo->nbInput; i++) {
    const TA_InputParameterInfo *paraminfo;
    TA_GetInputParameterInfo(funcInfo->handle, i, &paraminfo);
    switch (paraminfo->type) {
    case TA_Input_Price:
      if ((paraminfo->flags & TA_IN_PRICE_OPEN) && !open) {
        HANDLE_REQUIRED_ERROR("open")
      }
      if ((paraminfo->flags & TA_IN_PRICE_HIGH) && !high) {
        HANDLE_REQUIRED_ERROR("high")
      }
      if ((paraminfo->flags & TA_IN_PRICE_LOW) && !low) {
        HANDLE_REQUIRED_ERROR("low")
      }
      if ((paraminfo->flags & TA_IN_PRICE_CLOSE) && !close) {
        HANDLE_REQUIRED_ERROR("close")
      }
      if ((paraminfo->flags & TA_IN_PRICE_VOLUME) && !volume) {
        HANDLE_REQUIRED_ERROR("volume")
      }
      if ((paraminfo->flags & TA_IN_PRICE_OPENINTEREST) && !openInterest) {
        HANDLE_REQUIRED_ERROR("openInterest")
      }
      if ((retCode = TA_SetInputParamPricePtr(funcParams, i, open, high, low, close, volume, openInterest)) != TA_SUCCESS) {
        HANDLE_ERROR_CODE(retCode)
      }
      break;
    case TA_Input_Real:
      if (!inReal) {
        HANDLE_REQUIRED_ERROR("inReal")
      }
      if ((retCode = TA_SetInputParamRealPtr(funcParams, i, &inReal[i * step])) != TA_SUCCESS) {
        HANDLE_ERROR_CODE(retCode)
      }
      break;
    case TA_Input_Integer:
      if (!inInteger) {
        HANDLE_REQUIRED_ERROR("inInteger")
      }
      if ((retCode = TA_SetInputParamIntegerPtr(funcParams, i, &inInteger[i * step])) != TA_SUCCESS) {
        HANDLE_ERROR_CODE(retCode)
      }                            
      break;
    }
  }

  for (unsigned int i = 0; i < funcInfo->nbOptInput; i++) {
    const TA_OptInputParameterInfo *paraminfo;
    TA_GetOptInputParameterInfo(funcInfo->handle, i, &paraminfo);
    switch (paraminfo->type) {
    case TA_OptInput_RealRange:
    case TA_OptInput_RealList:
      if ((retCode = TA_SetOptInputParamReal(funcParams, i, optParams[i])) != TA_SUCCESS) {
        HANDLE_ERROR_CODE(retCode)
      }
      break;
    case TA_OptInput_IntegerRange:
    case TA_OptInput_IntegerList:
      if ((retCode = TA_SetOptInputParamInteger(funcParams, i, (int)optParams[i])) != TA_SUCCESS) {
        HANDLE_ERROR_CODE(retCode)
      }
      break;
    }
  }

  for (unsigned int i = 0; i < funcInfo->nbOutput; i++) {
    const TA_OutputParameterInfo *paramInfo;
    TA_GetOutputParameterInfo(funcInfo->handle, i, &paramInfo);
    switch(paramInfo->type) {
    case TA_Output_Real:
      if ((retCode = TA_SetOutputParamRealPtr(funcParams, i, &outReal[i * step])) != TA_SUCCESS) {
        HANDLE_ERROR_CODE(retCode)
      }
      break;
    case TA_Output_Integer:
      if ((retCode = TA_SetOutputParamIntegerPtr(funcParams, i, &outInteger[i * step])) != TA_SUCCESS) {
        HANDLE_ERROR_CODE(retCode)
      }
      break;
    }  
  }

  retCode = TA_CallFunc(funcParams, startIdx, endIdx, &outRange[0], &outRange[1]);
  // printf("outBegIdx:%d, outNBElement:%d\n", outRange[0], outRange[1]);
  TA_ParamHolderFree(funcParams);  
  return retCode == TA_SUCCESS ? NULL : handleErrorCode(retCode);
}

EMSCRIPTEN_KEEPALIVE
char *FuncList()
{
  TA_StringTable *groupTable;
  TA_StringTable *funcTable;

  yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val *root = yyjson_mut_arr(doc);
  yyjson_mut_doc_set_root(doc, root);

  if (TA_GroupTableAlloc(&groupTable) == TA_SUCCESS) {
    for (unsigned int i = 0; i < groupTable->size; i++) {
      if (TA_FuncTableAlloc(groupTable->string[i], &funcTable) == TA_SUCCESS) {
        for (unsigned int j = 0; j < funcTable->size; j++) {
          yyjson_mut_arr_append(root, yyjson_mut_str(doc, funcTable->string[j]));
        }
        TA_FuncTableFree(funcTable);
      }
    }
    TA_GroupTableFree(groupTable);
  }

  char *json = yyjson_mut_write(doc, 0, NULL);
  yyjson_mut_doc_free(doc);
  return json;
}

EMSCRIPTEN_KEEPALIVE
char *FuncDef(const char *funcName)
{
  const TA_FuncHandle *funcHandle;
  const TA_FuncInfo *funcInfo;
  char *ret = preTA(funcName, &funcHandle, &funcInfo);
  if (ret) return ret;

  yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  yyjson_mut_obj_add_str(doc, root, "name", funcInfo->name);
  yyjson_mut_obj_add_str(doc, root, "group", funcInfo->group);
  yyjson_mut_obj_add_str(doc, root, "hint", funcInfo->hint);
  
  yyjson_mut_val *inputs = yyjson_mut_arr(doc);
  yyjson_mut_obj_add_val(doc, root, "inputs", inputs);

  for (unsigned int i = 0; i < funcInfo->nbInput; i++) {
    const TA_InputParameterInfo *paraminfo;
    TA_GetInputParameterInfo(funcInfo->handle, i, &paraminfo);

    yyjson_mut_val *param = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, param, "name", paraminfo->paramName);
    switch(paraminfo->type) {
    case TA_Input_Price:
      yyjson_mut_obj_add_str(doc, param, "type", "price");
      break;
    case TA_Input_Real:
      yyjson_mut_obj_add_str(doc, param, "type", "real");
      break;
    case TA_Input_Integer:
      yyjson_mut_obj_add_str(doc, param, "type", "integer");
      break;
    }
    if (paraminfo->flags > 0) {
      yyjson_mut_val *flags = yyjson_mut_arr(doc);
      if (paraminfo->flags & TA_IN_PRICE_OPEN) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "open"));
      if (paraminfo->flags & TA_IN_PRICE_HIGH) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "high"));
      if (paraminfo->flags & TA_IN_PRICE_LOW) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "low"));
      if (paraminfo->flags & TA_IN_PRICE_CLOSE) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "close"));
      if (paraminfo->flags & TA_IN_PRICE_VOLUME) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "volume"));
      if (paraminfo->flags & TA_IN_PRICE_OPENINTEREST) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "open_interest"));
      if (paraminfo->flags & TA_IN_PRICE_TIMESTAMP) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "timestamp"));      
      yyjson_mut_obj_add_val(doc, param, "flags", flags);
    }
    yyjson_mut_arr_append(inputs, param);
  }

  yyjson_mut_val *optInputs = yyjson_mut_arr(doc); 
  yyjson_mut_obj_add_val(doc, root, "opt_inputs", optInputs);

  for (unsigned int i = 0; i < funcInfo->nbOptInput; i++) {
    const TA_OptInputParameterInfo *paraminfo;
    TA_GetOptInputParameterInfo(funcInfo->handle, i, &paraminfo);

    yyjson_mut_val *param = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, param, "name", paraminfo->paramName);
    yyjson_mut_obj_add_str(doc, param, "display_name", paraminfo->displayName);
    yyjson_mut_obj_add_real(doc, param, "default_value", paraminfo->defaultValue);
    yyjson_mut_obj_add_str(doc, param, "hint", paraminfo->hint);
    switch(paraminfo->type) {
    case TA_OptInput_RealRange:
      yyjson_mut_obj_add_str(doc, param, "type", "real");
      if (paraminfo->dataSet) {
        TA_RealRange *realRange = (TA_RealRange*)paraminfo->dataSet;
        yyjson_mut_val *range = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_real(doc, range, "min", realRange->min);
        yyjson_mut_obj_add_real(doc, range, "max", realRange->max);
        yyjson_mut_obj_add_int(doc, range, "precision", realRange->precision);
        yyjson_mut_obj_add_real(doc, range, "suggested_start", realRange->suggested_start);
        yyjson_mut_obj_add_real(doc, range, "suggested_end", realRange->suggested_end);
        yyjson_mut_obj_add_real(doc, range, "suggested_increment", realRange->suggested_increment);
        yyjson_mut_obj_add_val(doc, param, "range", range);
      }
      break;
    case TA_OptInput_RealList:
      yyjson_mut_obj_add_str(doc, param, "type", "real");
      if (paraminfo->dataSet) {
        yyjson_mut_val *list = yyjson_mut_obj(doc);
        TA_RealList *realList = (TA_RealList*) paraminfo->dataSet;
        for (unsigned int i = 0; i < realList->nbElement; i++) {
          yyjson_mut_obj_add_real(doc, list, realList->data[i].string, realList->data[i].value);
        }
        yyjson_mut_obj_add_val(doc, param, "list", list);
      }
      break;
    case TA_OptInput_IntegerRange:
      yyjson_mut_obj_add_str(doc, param, "type", "integer");
      if (paraminfo->dataSet) {
        TA_IntegerRange *integerRange = (TA_IntegerRange*)paraminfo->dataSet;
        yyjson_mut_val *range = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_int(doc, range, "min", integerRange->min);
        yyjson_mut_obj_add_int(doc, range, "max", integerRange->max);
        yyjson_mut_obj_add_int(doc, range, "suggested_start", integerRange->suggested_start);
        yyjson_mut_obj_add_int(doc, range, "suggested_end", integerRange->suggested_end);
        yyjson_mut_obj_add_int(doc, range, "suggested_increment", integerRange->suggested_increment);
        yyjson_mut_obj_add_val(doc, param, "range", range);
      }
      break;
    case TA_OptInput_IntegerList:
      yyjson_mut_obj_add_str(doc, param, "type", "integer");
      if (paraminfo->dataSet) {
        yyjson_mut_val *list = yyjson_mut_obj(doc);
        TA_IntegerList *integerList = (TA_IntegerList*) paraminfo->dataSet;
        for (unsigned int i = 0; i < integerList->nbElement; i++) {
          yyjson_mut_obj_add_int(doc, list, integerList->data[i].string, integerList->data[i].value);
        }
        yyjson_mut_obj_add_val(doc, param, "list", list);
      }
      break;
    }
    if (paraminfo->flags > 0) {
      yyjson_mut_val *flags = yyjson_mut_arr(doc);
      if (paraminfo->flags & TA_OPTIN_IS_PERCENT) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "percent"));
      if (paraminfo->flags & TA_OPTIN_IS_DEGREE) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "degree"));
      if (paraminfo->flags & TA_OPTIN_IS_CURRENCY) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "currency"));
      if (paraminfo->flags & TA_OPTIN_ADVANCED) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "advanced"));
      yyjson_mut_obj_add_val(doc, param, "flags", flags);
    }
    yyjson_mut_arr_append(optInputs, param);
  }

  yyjson_mut_val *outputs = yyjson_mut_arr(doc);
  yyjson_mut_obj_add_val(doc, root, "outputs", outputs);

  for (unsigned int i = 0; i < funcInfo->nbOutput; i++) {
    const TA_OutputParameterInfo *paraminfo;    
    TA_GetOutputParameterInfo(funcInfo->handle, i, &paraminfo);

    yyjson_mut_val *param = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, param, "name", paraminfo->paramName);
    switch(paraminfo->type) {
    case TA_Output_Real:
      yyjson_mut_obj_add_str(doc, param, "type", "real");
      break;
    case TA_Output_Integer:
      yyjson_mut_obj_add_str(doc, param, "type", "integer");
      break;
    }
    if (paraminfo->flags > 0) {
      yyjson_mut_val *flags = yyjson_mut_arr(doc);
      if (paraminfo->flags & TA_OUT_LINE) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "line"));
      if (paraminfo->flags & TA_OUT_DOT_LINE) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "dot_line"));
      if (paraminfo->flags & TA_OUT_DASH_LINE) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "dash_line"));
      if (paraminfo->flags & TA_OUT_DOT) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "dot"));
      if (paraminfo->flags & TA_OUT_HISTO) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "histogram"));
      if (paraminfo->flags & TA_OUT_PATTERN_BOOL) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "pattern_bool"));
      if (paraminfo->flags & TA_OUT_PATTERN_BULL_BEAR) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "pattern_bull_bear"));
      if (paraminfo->flags & TA_OUT_PATTERN_STRENGTH) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "pattern_strength"));
      if (paraminfo->flags & TA_OUT_POSITIVE) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "positive"));
      if (paraminfo->flags & TA_OUT_NEGATIVE) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "negative"));
      if (paraminfo->flags & TA_OUT_ZERO) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "zero"));
      if (paraminfo->flags & TA_OUT_UPPER_LIMIT) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "upper_limit"));
      if (paraminfo->flags & TA_OUT_LOWER_LIMIT) yyjson_mut_arr_append(flags, yyjson_mut_str(doc, "lower_limit"));
      yyjson_mut_obj_add_val(doc, param, "flags", flags);
    }
    yyjson_mut_arr_append(outputs, param);
  }

  char *json = yyjson_mut_write(doc, 0, NULL);
  yyjson_mut_doc_free(doc);
  return json;
}

EMSCRIPTEN_KEEPALIVE
char *TA(const char *funcName, int startIdx, int endIdx, int step,
         double open[], double high[], double low[], double close[],
         double volume[], double openInterest[],
         double inReal[], int inInteger[], double optParams[],
         double outReal[], int outInteger[], int outRange[])
{
  const TA_FuncHandle *funcHandle;
  const TA_FuncInfo *funcInfo;
  char *ret = preTA(funcName, &funcHandle, &funcInfo);
  if (ret) return ret;
  return ta(funcHandle, funcInfo,
            funcName, startIdx, endIdx, step,
            open, high, low, close, volume, openInterest,
            inReal, inInteger, optParams,
            outReal, outInteger, outRange);
}

EMSCRIPTEN_KEEPALIVE
char *TAL(const char *funcName, int step,
          double open[], double high[], double low[], double close[],
          double volume[], double openInterest[],
          double inReal[], int inInteger[], double optParams[],
          double outReal[], int outInteger[])
{
  const TA_FuncHandle *funcHandle;
  const TA_FuncInfo *funcInfo;
  char *ret = preTA(funcName, &funcHandle, &funcInfo);
  if (ret) return ret;

  int outRange[] = { 0, 0 };
  ret = ta(funcHandle, funcInfo,
           funcName, 0, step - 1, step,
           open, high, low, close, volume, openInterest,
           inReal, inInteger, optParams,
           outReal, outInteger, outRange);
  if (!ret) {
    int outBegIdx = outRange[0];
    int outNBElement = outRange[1];
    if (step == outBegIdx + outNBElement && outBegIdx > 0) {
      for (unsigned int i = 0; i < funcInfo->nbOutput; i++) {
        if (outReal) shiftReal(outReal, i * step, outBegIdx, outNBElement);
        if (outInteger) shiftInteger(outInteger, i * step, outBegIdx, outNBElement);
      }
    }
  }
  return ret;
}

EMSCRIPTEN_KEEPALIVE
char *SetUnstablePeriod(int funcId, int unstablePeriod)
{   
  TA_RetCode retCode = TA_SetUnstablePeriod((TA_FuncUnstId)funcId, unstablePeriod);
  return retCode == TA_SUCCESS ? NULL : handleErrorCode(retCode);
}

EMSCRIPTEN_KEEPALIVE
void release(void *ptr)
{
  free(ptr);
}

int main()
{
  return TA_Initialize() == TA_SUCCESS ? 0 : 1;
}

#include "Algebra.h"
#include <cstring>
#include <iostream>
using namespace std;

/* used to select all the records that satisfy a condition.
the arguments of the function are
- srcRel - the source relation we want to select from
- targetRel - the relation we want to select into. (ignore for now)
- attr - the attribute that the condition is checking
- op - the operator of the condition
- strVal - the value that we want to compare against (represented as a string)

*/

bool isNumber(char *str) {
  int len;
  float ignore;
  /*
    sscanf returns the number of elements read, so if there is no float matching
    the first %f, ret will be 0, else it'll be 1

    %n gets the number of characters read. this scanf sequence will read the
    first float ignoring all the whitespace before and after. and the number of
    characters read that far will be stored in len. if len == strlen(str), then
    the string only contains a float with/without whitespace. else, there's
    other characters.
  */
  int ret = sscanf(str, "%f %n", &ignore, &len);
  return ret == 1 && len == strlen(str);
}

/*!
This function creates a new target relation with attributes as that of source
relation. It inserts the records of source relation which satisfies the given
condition into the target Relation.
*/

int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE],
                    char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]) {
  int srcRelId = OpenRelTable::getRelId(srcRel); // we'll implement this later
  if (srcRelId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  AttrCatEntry attrCatEntry;
  // get the attribute catalog entry for attr, using
  // AttrCacheTable::getAttrcatEntry()
  //    return E_ATTRNOTEXIST if it returns the error
  if (AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry) ==
      E_ATTRNOTEXIST) {
    return E_ATTRNOTEXIST;
  }

  /*** Convert strVal (string) to an attribute of data type NUMBER or STRING
   * ***/
  int type = attrCatEntry.attrType;
  Attribute attrVal;
  if (type == NUMBER) {
    if (isNumber(strVal)) { // the isNumber() function is implemented below
      attrVal.nVal = atof(strVal);
    } else {
      return E_ATTRTYPEMISMATCH;
    }
  } else if (type == STRING) {
    strcpy(attrVal.sVal, strVal);
  }

  // Prepare arguments for createRel() in the following way:
  // get RelcatEntry of srcRel using RelCacheTable::getRelCatEntry()
  RelCatEntry srcRelCatEntry;
  RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
  int src_nAttrs = srcRelCatEntry.numAttrs;

  char attr_names[src_nAttrs][ATTR_SIZE];
  int attr_types[src_nAttrs];

  for (int i = 0; i < src_nAttrs; i++) {
    AttrCatEntry srcAttrCatEntry;
    AttrCacheTable::getAttrCatEntry(srcRelId, i, &srcAttrCatEntry);
    strcpy(attr_names[i], srcAttrCatEntry.relName);
    attr_types[i] = srcAttrCatEntry.attrType;
  }

  int ret = Schema::createRel(targetRel, src_nAttrs, attr_names, attr_types);
  if (ret != SUCCESS)
    return SUCCESS;

  int targetRelId = OpenRelTable::openRel(targetRel);
  if (targetRelId < 0 or targetRelId >= MAX_OPEN) {
    Schema::deleteRel(targetRel);
    return targetRelId;
  }
  // RelCacheTable::resetSearchIndex(targetRelId);
  Attribute record[src_nAttrs];

  RelCacheTable::resetSearchIndex(srcRelId);
  AttrCacheTable::resetSearchIndex(srcRelId, attr);

  while (BlockAccess::search(srcRelId, record, attr, attrVal, op) == SUCCESS) {
    ret = BlockAccess::insert(targetRelId, record);
    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  Schema::closeRel(targetRel);

  return SUCCESS;
}

// will return if a string can be parsed as a floating point number

int Algebra::insert(char relName[ATTR_SIZE], int nAttrs,
                    char record[][ATTR_SIZE]) {

  if (strcmp(relName, RELCAT_RELNAME) == 0 or
      strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return E_NOTPERMITTED;
  }

  // get the relation's rel-id using OpenRelTable::getRelId() method
  int relId = OpenRelTable::getRelId(relName);

  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  RelCatEntry relCatBuf;
  RelCacheTable::getRelCatEntry(relId, &relCatBuf);

  // if relation is not open in open relation table, return E_RELNOTOPEN
  // (check if the value returned from getRelId function call = E_RELNOTOPEN)
  // get the relation catalog entry from relation cache
  // (use RelCacheTable::getRelCatEntry() of Cache Layer)

  /* if relCatEntry.numAttrs != numberOfAttributes in relation,
     return E_NATTRMISMATCH */
  if (relCatBuf.numAttrs != nAttrs) {
    return E_NATTRMISMATCH;
  }

  // let recordValues[numberOfAttributes] be an array of type union Attribute
  Attribute recordValues[nAttrs];

  /*
      Converting 2D char array of record values to Attribute array recordValues
   */
  // iterate through 0 to nAttrs-1: (let i be the iterator)
  for (int i = 0; i < nAttrs; i++) {
    // get the attr-cat entry for the i'th attribute from the attr-cache
    // (use AttrCacheTable::getAttrCatEntry())
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);
    int type = attrCatEntry.attrType;
    // let type = attrCatEntry.attrType;

    if (type == NUMBER) {
      // if the char array record[i] can be converted to a number
      // (check this using isNumber() function)
      if (isNumber(record[i])) {
        /* convert the char array to numeral and store it
           at recordValues[i].nVal using atof() */
        recordValues[i].nVal = atof(record[i]);
      } else {
        return E_ATTRTYPEMISMATCH;
      }
    } else if (type == STRING) {
      // copy record[i] to recordValues[i].sVal
      strcpy(recordValues[i].sVal, record[i]);
    }
  }
  int retVal = BlockAccess::insert(relId, recordValues);
  // insert the record by calling BlockAccess::insert() function
  // let retVal denote the return value of insert call

  return retVal;
}

/*
This function creates a copy of the source relation in the target relation.
 Every record of the source relation is inserted into the target relation.
*/
int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]) {
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if (srcRelId < 0 or srcRelId >= MAX_OPEN) {
    return E_RELNOTOPEN;
  }
  RelCatEntry srcRelCatEntry;
  RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);

  int numAttrs = srcRelCatEntry.numAttrs;

  char attrNames[numAttrs][ATTR_SIZE];
  int attrTypes[numAttrs];
  for (int i = 0; i < numAttrs; i++) {
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
    strcpy(attrNames[i], attrCatEntry.attrName);
    attrTypes[i] = attrCatEntry.attrType;
  }

  /*** Creating and opening the target relation ***/
  int ret = Schema::createRel(targetRel, numAttrs, attrNames, attrTypes);
  if (ret != SUCCESS)
    return ret;

  int targetrelId = OpenRelTable::openRel(targetRel);
  if (targetrelId < 0 or targetrelId >= MAX_OPEN) {
    Schema::deleteRel(targetRel);
    return targetrelId;
  }

  /***Inserting projected records into the target relation ***/
  RelCacheTable::resetSearchIndex(targetrelId);
  Attribute record[numAttrs];
  while (BlockAccess::project(srcRelId, record) == SUCCESS) {
    ret = BlockAccess::insert(targetrelId, record);
    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }
  Schema::closeRel(targetRel);
  return SUCCESS;
}

/*
This function creates a new target relation with list of attributes specified in
the arguments. For each record of the source relation, it inserts a new record
into the target relation with the attribute values corresponding to the
attributes specified in the attribute list.
*/

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE],
                     int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]) {
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if (srcRelId < 0 or srcRelId >= MAX_OPEN) {
    return E_RELNOTOPEN;
  }
  RelCatEntry srcRelCatEntry;
  RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
  int numAttrs = srcRelCatEntry.numAttrs;
  int attr_offset[tar_nAttrs];
  int attr_types[tar_nAttrs];

  /*** Checking if attributes of target are present in the source relation
       and storing its offsets and types ***/
  for (int i = 0; i < tar_nAttrs; i++) {
    AttrCatEntry attrCatEntry;
    int ret =
        AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[i], &attrCatEntry);
    if (ret != SUCCESS) {
      return E_ATTRNOTEXIST;
    }
    attr_offset[i] = attrCatEntry.offset;
    attr_types[i] = attrCatEntry.attrType;
  }

  /*** Creating and opening the target relation ***/
  int ret = Schema::createRel(targetRel, tar_nAttrs, tar_Attrs, attr_types);
  if (ret != SUCCESS) {
    return ret;
  }

  int targetRelId = OpenRelTable::openRel(targetRel);
  if (targetRelId < 0 or targetRelId >= MAX_OPEN) {
    Schema::deleteRel(targetRel);
    return targetRelId;
  }

  /*** Inserting projected records into the target relation ***/
  RelCacheTable::resetSearchIndex(srcRelId);
  Attribute record[numAttrs];

  while (BlockAccess::project(srcRelId, record) == SUCCESS) {
    Attribute proj_record[tar_nAttrs];
    for (int i = 0; i < tar_nAttrs; i++) {
      proj_record[i] = record[attr_offset[i]];
    }

    ret = BlockAccess::insert(targetRelId, proj_record);
    if (ret != SUCCESS) {
      printf("insertion into target relation  failed");
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }
  Schema::closeRel(targetRel);
  return SUCCESS;
}
/*==========================================================================

      copystr.cpp  -  Don Cross, November 1993.

      Contains simple string copy and delete routines.

==========================================================================*/

#include <string.h>
#include <copystr.h>


char *DDC_CopyString ( const char *string )
{
   char *copy = new char [strlen(string) + 1];
   if ( copy )
   {
      strcpy ( copy, string );
   }
   return copy;
}


void DDC_DeleteString ( char * &string )
{
   if ( string )
   {
      delete[] string;
      string = 0;
   }
}


/*--- end of file copystr.cpp ---*/


int main(){

  int fun;

  int (*fun_ptr)(int) = &fun;
  int c = (*fun_ptr)(10);
    
  for (int i=0; i<3; i++){
        c = c + 1;
  }
  return c;
}
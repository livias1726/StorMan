#include "storman.h"

/*
Acquisisce un puntatore.
@params
	ptr_addr: indirizzo del puntatore di cui si vuole cambiare il valore
	val: valore che si vuole assegnare al puntatore puntato da ptr_addr
	mptr_addr: l’indirizzo di un puntatore gestito
@return
	0: esecuzione corretta
	1: val non è un indirizzo di un blocco gestito da storman
*/
int pointer_assign(void** ptr_addr, void* val, void** mptr_addr){

	void* start;
	void* end;
	int num_ptr;

	//STEP 1-2
	//Se *ptr_addr non fa parte di uno dei blocchi gestiti o non è in uno snapshot 
	//allora pointer_assign si comporta come nella versione della parte A.
	if((retrieve_block(*ptr_addr, available_zones, &start, &end) == -1) || 
		((num_ptr = has_multiple_ptr(*ptr_addr, handled_ptrs, available_zones)) == 1)){
			return pointer_assign_internal(ptr_addr, val);
	}

	//STEP 3
	//Se l’indirizzo in ptr_addr fa parte di un blocco B gestito da storman e B è in uno snapshot

	//1. Verifica che mptr_addr sia l’indirizzo di un puntatore appartenente allo snapshot di B. In caso contrario return 2.
	void** snapshot = retrieve_snapshot(handled_ptrs, start, end, num_ptr);
	if(!is_in_snapshot(*mptr_addr, snapshot, num_ptr)){
		return 2;
	}

	//2. Alloca una copia di B in B'.

	//Allineamento
	size_t alignment = retrieve_alignment(start);

	//Allocazione
	size_t size = (size_t)(end-start);
	block_alloc(mptr_addr, alignment, size);

	//Copia blocco
	void* newstart;
	void* newend;	
	retrieve_block(*mptr_addr, available_zones, &newstart, &newend);
	copy_block_content(newstart, start, size);

	//Copia puntatori
	size_t num;
	void*** pointer_array = block_info(ptr_addr, &start, &end, &num);
	if(num != 0){
		insert_corresp_ptrs(pointer_array, start, (int)num, *mptr_addr);
	}

	//3. Esegui pointer_assign sul corrispondente di *ptr_addr in B'.
	return pointer_assign_internal(mptr_addr, val);
}

/*
Params:
	ptr_addr:
Return
	0: esecuzione corretta
	1: errore, il puntatore con indirizzo in ptr_addr non è gestito da storman.
*/

int toggle_snapshot(void** ptr_addr){
	//STEP 1
	//Se il puntatore con indirizzo in ptr_addr non è gestito da storman, return 1.
	if(!is_handled(*ptr_addr, handled_ptrs)){
		return 1;
	}

	void* beginning;
	void* end;
	size_t num;
	//STEP 2
	//Se il puntatore con indirizzo in ptr_addr fa già parte di uno snapshot, trasforma lo snapshot in un alias group e return 0.
	void*** group = block_info(ptr_addr, &beginning, &end, &num);
	if(is_a_snapshot(group, num)){
		//snapshot_to_alias();
		printf("snap\n");
	}else{
		//Step 3
		//Se il puntatore con indirizzo in ptr_addr fa parte di un alias group allora trasforma l’alias group in snapshot e return 0.
		//alias_to_snapshot();
		printf("alias\n");
	}
	return 0;
}

/*
Params:
	pointers: l’indirizzo di un array di void **, allocato con malloc, contenente indirizzi di puntatori gestiti da storman.
	num_ptrs: il numero di elementi dell’array puntato da pointers.
Return:
	0: esecuzione corretta
	1: errore, esiste almeno un puntatore in pointers non gestito da storman.
	2: errore, esiste almeno un puntatore in pointers che faccia parte di uno snapshot.
*/
int dedup_blocks(void*** pointers, int num_ptrs){

	//STEP 1
	//Se almeno uno dei puntatori in pointers non è fra quelli gestiti da storman, return 1.
	int i;
	for(i=0; i<num_ptrs; i++){
		if(!is_handled(*pointers[i], handled_ptrs)){
			return 1;
		}
	}

	//STEP 2
	//Se almeno uno dei puntatori in pointers fa parte di una snapshot, return 2.
	void* start;
	void* end;
	size_t n;
	void* starts[num_ptrs];
	void* ends[num_ptrs];
	int count = 0;
	void*** group;

	for(i=0; i<num_ptrs; i++){
		group = block_info(pointers[i], &start, &end, &n);		
		if(is_a_snapshot(group, n)){
			free(group);
			return 2;
		}
		free(group);

		//Siano (B1, ..., Bk) i blocchi puntati dai puntatori in pointers... 
		starts[count] = start;
		ends[count] = end;
		count++;
	}

	//...ed elencati secondo il loro ordine relativo in memoria
	reorder_addresses(starts, ends, count, num_ptrs);

	//STEP 3
	//Ripartisce (B1, ..., Bk) in (S1, ..., Sh) insiemi tali che, per ogni Bi, Bj e St:
	int num_set = 0;	
	int* in_set;
	void**** sets = group_duplicates(starts, ends, count, &num_set, &in_set);

	//STEP 4
	//Per ogni St:
	//Sceglie un blocco Bi in St come rappresentante -> B(St).
	int j;
	void* s;
	void* e;
	void*** pointer_array;

	for(i=0; i<num_set; i++){
		start = sets[i][0][0];
		end = sets[i][1][0];

		//Per ogni B in {St − B(St)}
		for(j=1; j<in_set[i]; j++){
			s = sets[i][0][j];
			e = sets[i][1][j];

			//Cambia tutti i puntatori che puntano a B in modo che puntino a B(St)
			pointer_array = block_info(&s, &s, &e, &n);
			if(n != 0){
				insert_corresp_ptrs(pointer_array, s, n, start);
			}			
			free(pointer_array);

			//Dealloca B.
			release_block(s, &available_zones);
		}

		free(sets[i]);
	}

	free(sets);
	free(in_set);

	return 0;
}
*Progetto di Laboratorio di Sistemi Operativi* – Università di Pisa –
Copyright (c) 2015 Giorgio Vinciguerra

Informazioni generali
=====================
Questa cartella contiene i file per il collaudo dei moduli del progetto. Per la libreria wator viene usato il framework di unit testing [Unity](https://github.com/ThrowTheSwitch/Unity). Per il processo wator vengono usati gli script bash planet_generator e test_process.


Descrizione del contenuto della cartella
----------------------------------------
- **test_wator.c** contiene i test case per la libreria wator.
- **test_runners/** contiene file (generati automaticamente) che lanciano i test case.
- **unity_framework/** contiene i sorgenti del framework Unity e una breve descrizione delle API (file Unity README.md).
- **test_data/** contiene i file di supporto per l'esecuzione dei test case (file di input, di configurazione del programma...).
- **Makefile** il file per la compilazione e l'esecuzione dei test sulla librearia.
- **planet_generator.sh** lo script per generare un pianeta casuale.
- **test_process.sh** lo script per il test del processo.

Eseguire il test della libreria
-------------------------------
Per eseguire un test, posizionarsi in questa cartella con il comando `cd` e lanciare il comando `make`.

Il test runner, cioè il file che esegue i vari test case, viene generato automaticamente dallo script `unity_framework/auto/generate_test_runner.rb`. Questo script riceve come parametro il file `test_wator.c`, cerca tutte le funzioni che iniziano con "test_" ed inserisce una chiamata ad ognuna di queste funzioni nel `main()` del test runner. Il runner viene quindi compilato e lanciato: l'esito del test comparirà sullo schermo.


Eseguire il test del processo
-----------------------------
Posizionarsi in questa cartella ed eseguire: `bash test_process.sh numero_iterazioni durata_singola_iterazione n m`

Il primo parametro è il numero di simulazioni da eseguire. Il secondo parametro è la durata di ogni singola  simulazione prima dell'invio del segnale di terminazione. Il terzo e il quarto parametro sono le dimensioni della matrice di simulazione. Al termine del test verrà stampata la media del tempo di esecuzione delle simulazioni.

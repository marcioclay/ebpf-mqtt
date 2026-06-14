
##  📊 Guia de Execução de Testes: Cenário 2 (slowITE)

![Tcpdump](https://img.shields.io/badge/Tcpdump-network%20analysis-orange?style=flat-square&logo=wireshark)
![Hping3](https://img.shields.io/badge/Hping3-packet%20crafting-red?style=flat-square&logo=linux)
![DDoS](https://img.shields.io/badge/DDoS%20%26%20Slow-attack%20simulation-darkred?style=flat-square&logo=apache)
![Cybersecurity](https://img.shields.io/badge/Cybersecurity-lab%20focus-blue?style=flat-square&logo=security)
![IoT](https://img.shields.io/badge/IoT-connected%20devices-lightblue?style=flat-square&logo=internetofthings)


Este guia orienta a validação do protótipo através do estabelecimento de tráfego legítimo, simulação de ataque de inundação e extração de metricas diretamente do plano de dados.

### 2. Métricas para Ataques Slow DoS (Camada de Aplicação / SlowITe)
* **2.1. Contagem de Conexões TCP Simultâneas por IP**(INSERIR sockops)
    * *Descrição:* Quantificação de sockets abertos e mantidos ativos por um único endereço de origem.
    * *Aplicação:* Detecção de exaustão de *slots* limitados no Broker MQTT (*max_connections*).
* **2.2. Persistência e Tempo de Vida da Conexão (*Timestamps / Last Seen*)**
    * *Descrição:* Registo do tempo decorrido desde a abertura do socket sem que haja atividade útil de transmissão.
    * *Aplicação:* Identificação de sessões dormentes ou conexões presas intencionalmente.
* **2.3. Tamanho Médio dos Pacotes (*Payload Anomaly*)**
    * *Descrição:* Análise do tamanho em bytes das mensagens recebidas na camada de transporte/aplicação.
    * *Aplicação:* Detecção de anomalias por envio de pacotes truncados, incompletos ou contendo apenas bytes de controlo (ex: apenas o byte inicial `\x10`).
* **2.4. Densidade e Frequência de Pacotes ao Longo do Tempo**
    * *Descrição:* Avaliação do intervalo de tempo entre as transmissões dentro de uma mesma sessão TCP.
    * *Aplicação:* Identificação de comportamento silencioso (baixa taxa de transferência), projetado para contornar os mecanismos de *timeout* padrão da aplicação.

	  ```
		" O protótipo atua na camada de driver (XDP). Ele identifique a anomalia de payload e frequência do Slow DoS,
    	mas é incapaz de validar se a aplicação (MQTT) manteve os sockets abertos (Métrica 2.1).
    	Isso comprova a necessidade de acoplar ganchos na camada de transporte (sockops).
      ```

### Índice de Testes

Ao reiniciar o laboratório o Kernel do Linux é completamente zerado, isto significa que os contêineres foram parados e o programa eBPF foi apagado da memória. Caso esse seja o caso, siga essa etapas: 

```
sudo containerlab deploy -t topologia.yml --reconfigure
```
```
# executar o script para reinstalar o Mosquitto, o Python e a biblioteca do MQTT
./setup.sh
```
```
# 1. Garantir que o diretório de pins antigo seja limpo
sudo docker exec -it clab-lab-ebpf-gateway rm -f /sys/fs/bpf/xdp_monitor_test

# 2. Carregar e pinar o programa de novo
sudo docker exec -it clab-lab-ebpf-gateway bpftool prog load /lab/xdp_monitor.o /sys/fs/bpf/xdp_monitor_test type xdp

# 3. Anexar o filtro à interface eth1
sudo docker exec -it clab-lab-ebpf-gateway ip link set dev eth1 xdpgeneric pinned /sys/fs/bpf/xdp_monitor_test
```

### Passo 1: Habilitar conexões e testes MQTT


```
# Executar e Validar a Conexão
No Terminal 1 (Ativar o receptor no nó atacante):

sudo docker exec -it clab-lab-ebpf-atacante python3 /src/subscriber.py
# Conectar e aguardar mensagem
```

```
No Terminal 2 (Ativar o emissor no nó sensor):

sudo docker exec -it clab-lab-ebpf-sensor python3 /src/sensor.py
```


---

### Passo 2: Ataque slowITE

Terminal A -  Janela de Observação

Mapas - proto_stats e tcp_sessions

```
# Deixe este comando rodando na tela. Ele vai atualizar sozinho.
watch -n 1 sudo docker exec -it clab-lab-ebpf-gateway bpftool map dump id 55
```

#### Caso tenha reiniciado será necessário atualizar o id do mapa.
```
sudo docker exec -it clab-lab-ebpf-gateway bpftool map list
```
```
# Mensagem na tela
120: hash  name proto_stats  flags 0x0
	key 4B  value 8B  max_entries 10  memlock 4096B
121: hash  name ip_stats     flags 0x0
	key 4B  value 16B  max_entries 1024  memlock 8192B
# Procure pelos nomes dos seus mapas (como proto_stats).
# O número que aparece logo no início da linha (ex: 120, 121) é o seu novo ID.
```

Terminal B - Ativar sensor legítimo - nó sensor

```
sudo docker exec -it clab-lab-ebpf-sensor python3 /src/sensor.py
```

Terminal C - Ativar ataque slowITE no atacante

```
sudo docker exec -it clab-lab-ebpf-atacante python3 /src/ataque_slow.py
```
--- 

### Resolução de problemas MQTT
#### Acesse o terminal do gateway:
```
docker exec -it clab-lab-ebpf-gateway /bin/bash
```
O mosquitto.conf está em /etc/mosquitto 

Inserir dados de conexões
```
echo -e "max_connections 1000\nglobal_max_clients 1000" > /etc/mosquitto/conf.d/testes.conf
```

```
kill -HUP $(cat /run/mosquitto/mosquitto.pid)
```
Ou, se estiver rodando como serviço tradicional:

```
service mosquitto restart
```

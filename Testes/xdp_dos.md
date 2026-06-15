
##  📊 Guia de Execução de Testes: Cenário 1 (DoS Volumétrico)

![Tcpdump](https://img.shields.io/badge/Tcpdump-network%20analysis-orange?style=flat-square&logo=wireshark)
![Hping3](https://img.shields.io/badge/Hping3-packet%20crafting-red?style=flat-square&logo=linux)
![DDoS](https://img.shields.io/badge/DDoS%20%26%20Slow-attack%20simulation-darkred?style=flat-square&logo=apache)
![Cybersecurity](https://img.shields.io/badge/Cybersecurity-lab%20focus-blue?style=flat-square&logo=security)
![IoT](https://img.shields.io/badge/IoT-connected%20devices-lightblue?style=flat-square&logo=internetofthings)


Este guia orienta a validação do protótipo através do estabelecimento de tráfego legítimo, simulação de ataque de inundação e extração de metricas diretamente do plano de dados.

### Métricas para Ataques DoS (Volumétricos)
* **A. Taxa de Pacotes por Segundo (PPS - *Packets Per Second*)**
    * *Descrição:* Monitorização de picos súbitos no volume de pacotes recebidos pela interface de rede.
    * *Aplicação:* Identificação de saturação de infraestrutura e ataques de inundação (Flooding).
* **B. Consumo de Banda (Mbps / Gbps)**
    * *Descrição:* Volume total de tráfego de dados por unidade de tempo.
    * *Aplicação:* Análise de esgotamento do link de comunicação do gateway.

### Deploy do laboratório

Ao reiniciar o laboratório o Kernel do Linux é completamente zerado, isto significa que os contêineres foram parados e o programa eBPF foi apagado da memória. Caso esse seja o caso, siga essa etapas: 

```
sudo containerlab deploy -t topologia.yml --reconfigure
```
```
# Permissão ao script
chmod +x setup.sh
```
```
# Instalação: Mosquitto, o Python, biblioteca do MQTT, iptables e tc(emulação wifi)
./setup.sh
```

--- 
### Passo 1: Dashboard de Observação

```
# Terminal A - Iniciar dashboard e mantenha ligado durante todo laboratório.
docker exec -it clab-lab-ebpf-gateway python3 /lab/src/dashboard.py
```

Comprova que os mecanismos de defesa não estão causando falsos positivos.
Enquanto sensor legítimo envia pacotes não há alteração significativa de PPS, Banda ou CPU.

```
# Terminal B - Gera trafego legítimo MQTT no gateway
sudo docker exec -it clab-lab-ebpf-sensor python3 /src/sensor.py
```

### Passo 2: Observação com iptables
Neste cenário, utilizamos o Firewall nativo do Linux como Sistema de Prevenção de Intrusões (IPS) contra um ataque de Flood UDP.

   2.1. Preparar o Gateway
   Certifique-se de que o XDP está desligado e aplique as regras de mitigação do iptables:

   ```
   # Desligar o eBPF/XDP da interface (se estiver ativo)
   sudo docker exec -it clab-lab-ebpf-gateway ip link set dev eth1 xdpgeneric off
   
   # Aplicar o script de regras do iptables - só aceita 10 pacotes UDP por segundo
   sudo docker exec -it clab-lab-ebpf-gateway /lab/regras_iptables.sh
   ``` 

   2.2. Iniciar o Ataque 
   
   O ataque deve ser feito em dois ou mais terminais, o ataque será Dos volumetrico com falsificação de ip com nó único.
   
   ```
   # Terminal C e D: Flood UDP utilizando hping3 - simula ip spoofing
   sudo docker exec -it clab-lab-ebpf-atacante hping3 --flood --rand-source --udp -d 120 -p 1883 10.0.0.1
   ```
   
   ---

   ### Passo 3: Observação com eBPF / XDP

   Fechar e reiniciar o dashboard para novo teste.

   ```
   # Terminal A - Iniciar dashboard e mantenha ligado durante todo laboratório.
   docker exec -it clab-lab-ebpf-gateway python3 /lab/src/dashboard.py
   ```
   
   Substituir o iptables pelo código eBPF .
   
   3.1. Preparar o Gateway
   ```
   #  Zerar iptables 
   sudo docker exec -it clab-lab-ebpf-gateway iptables -F
   ```
   ```
   #  Dar permissão de acesso ao script xdp
    chmod +x setup_xdp.sh
   ```
   ```
      # Carregar e anexar o XDP na interface eth1 (foi desativado no item 2.1)
      ./setup_xdp.sh
   ```
   
   3.2. Iniciar o Ataque
   
   ```
   # Terminal C e D: Flood UDP utilizando hping3 - simula ip spoofing
   sudo docker exec -it clab-lab-ebpf-atacante hping3 --flood --rand-source --udp -d 120 -p 1883 10.0.0.1
   ```
   
     
  





